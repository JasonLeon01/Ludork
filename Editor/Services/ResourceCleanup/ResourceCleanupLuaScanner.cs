using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace Ludork.Services;

internal static class ResourceCleanupLuaScanner
{
    private static readonly IReadOnlyDictionary<string, string> DataFunctions =
        new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["GetAnimation"] = "Animations", ["AddAnim"] = "Animations",
            ["AddAnimOn"] = "Animations", ["GetAnimLength"] = "Animations",
            ["GetAnimVisualLength"] = "Animations", ["GetTileset"] = "Tilesets",
            ["GetAutoTile"] = "AutoTiles", ["HasAutoTile"] = "AutoTiles",
            ["GetCurve"] = "Curves", ["GetVector2Curve"] = "Curves",
            ["GetVector3Curve"] = "Curves", ["GetVector4Curve"] = "Curves",
            ["GetPlainTextConfig"] = "TextConfigs", ["GetRichTextConfig"] = "TextConfigs",
            ["getPlainTextConfig"] = "TextConfigs", ["getRichTextConfig"] = "TextConfigs",
            ["GetParticle"] = "Particles", ["GetParticleData"] = "Particles",
            ["setParticle"] = "Particles",
            ["RunCommonFunction"] = "CommonFunctions", ["GetCommonFunction"] = "CommonFunctions",
            ["CreateActorFromBPPath"] = "Blueprints", ["CreateActorFromBPPathWithDefaults"] = "Blueprints",
            ["AddPlayerByClass"] = "Blueprints", ["RemovePlayerByClass"] = "Blueprints",
        };

    private static readonly IReadOnlyDictionary<string, string> AssetFunctions =
        new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["loadFont"] = "Fonts", ["loadShader"] = "Shaders", ["loadSound"] = "Sounds",
            ["playSE"] = "Sounds", ["playVoice"] = "Voices", ["playMusic"] = "Musics",
            ["loadBlock"] = "Blocks", ["loadCharacter"] = "Characters", ["loadSystem"] = "System",
            ["loadTileset"] = "Tilesets", ["loadAutotile"] = "Autotiles", ["loadFog"] = "Fogs",
            ["loadTransition"] = "Transitions", ["loadFullShaderWithGeo"] = "Shaders", ["loadGeoShader"] = "Shaders",
        };

    public static bool IsHandwrittenRuntime(string relativePath)
    {
        return relativePath.StartsWith("Scripts/", StringComparison.Ordinal)
            && relativePath.EndsWith(".lua", StringComparison.OrdinalIgnoreCase)
            && !relativePath.StartsWith("Scripts/stub/", StringComparison.Ordinal)
            && !relativePath.StartsWith("Scripts/Internal/UI/", StringComparison.Ordinal)
            && !relativePath.StartsWith("Scripts/Source/Locale/", StringComparison.Ordinal)
            && !relativePath.EndsWith(".d.lua", StringComparison.OrdinalIgnoreCase)
            && relativePath is not "Scripts/Source/Configs/GeneralEnum.lua"
                and not "Scripts/Source/Configs/GeneralDataTypes.lua"
                and not "Scripts/Engine_meta.lua" and not "Scripts/GlobalCore_meta.lua"
                and not "Scripts/GlobalFunctions_meta.lua";
    }

    public static IReadOnlySet<string> ReadReferences(string source, CancellationToken token)
    {
        List<Token> tokens = tokenize(source, token);
        Dictionary<string, HashSet<string>> constants = new(StringComparer.Ordinal);
        HashSet<string> emitterVariables = new(StringComparer.Ordinal);
        bool constantsChanged;
        do
        {
            constantsChanged = false;
            for (int index = 1; index + 1 < tokens.Count; index++)
            {
                token.ThrowIfCancellationRequested();
                if (tokens[index].Text != "=" || tokens[index].Kind != TokenKind.Symbol)
                    continue;
                IReadOnlyList<string?> names = readAssignmentNames(tokens, index);
                if (names.Count == 0)
                    continue;
                int position = index + 1;
                foreach (string? name in names)
                {
                    IReadOnlyList<string> inferred = readExpressionValues(tokens, ref position, tokens.Count, constants);
                    if (name is not null && inferred.Count != 0)
                    {
                        if (!constants.TryGetValue(name, out HashSet<string>? values))
                            constants[name] = values = new HashSet<string>(StringComparer.Ordinal);
                        foreach (string value in inferred)
                            constantsChanged |= values.Add(value);
                    }
                    if (position >= tokens.Count || tokens[position].Kind != TokenKind.Symbol
                        || tokens[position].Text != ",")
                        break;
                    position++;
                }
            }
        }
        while (constantsChanged);
        for (int index = 1; index + 3 < tokens.Count; index++)
        {
            if (tokens[index].Text != "=" || tokens[index].Kind != TokenKind.Symbol)
                continue;
            int constructor = index + 1;
            while (constructor + 2 < tokens.Count && tokens[constructor + 1].Text == "."
                && tokens[constructor + 2].Kind == TokenKind.Identifier)
                constructor += 2;
            if (tokens[constructor].Text == "new" && isEmitterConstructor(tokens, constructor))
                emitterVariables.Add(precedingName(tokens, index - 1));
        }
        HashSet<string> references = new(StringComparer.Ordinal);
        for (int index = 0; index < tokens.Count; index++)
        {
            token.ThrowIfCancellationRequested();
            Token current = tokens[index];
            if (current.Kind == TokenKind.String
                && (index == 0 || tokens[index - 1].Text != "..")
                && (index + 1 == tokens.Count || tokens[index + 1].Text != ".."))
                addExplicitReference(references, current.Text);
            if (current.Kind != TokenKind.Identifier)
                continue;
            if (current.Text == "Data")
            {
                StringBuilder dotted = new("Data");
                for (int part = index + 1; part + 1 < tokens.Count
                    && tokens[part].Text == "." && tokens[part + 1].Kind == TokenKind.Identifier; part += 2)
                {
                    dotted.Append('.').Append(tokens[part + 1].Text);
                    addExplicitReference(references, dotted.ToString());
                }
            }
            if (index + 1 >= tokens.Count
                || tokens[index + 1].Text != "(" && tokens[index + 1].Kind != TokenKind.String)
                continue;
            bool dataFunction = DataFunctions.TryGetValue(current.Text, out string? section);
            if (isEmitterConstructor(tokens, index)
                || current.Text == "load" && index >= 2
                    && emitterVariables.Contains(precedingName(tokens, index - 2)))
            {
                dataFunction = true;
                section = "Particles";
            }
            bool assetFunction = AssetFunctions.TryGetValue(current.Text, out string? folder);
            if (!dataFunction && !assetFunction && current.Text != "loadTexture")
                continue;
            IReadOnlyList<IReadOnlyList<string>> arguments = tokens[index + 1].Kind == TokenKind.String
                ? [[tokens[index + 1].Text]]
                : readArguments(tokens, index + 1, constants);
            if (current.Text == "loadTexture" && arguments.Count >= 2)
            {
                foreach (string subFolder in arguments[0])
                {
                    foreach (string filename in arguments[1])
                        addAssetReference(references, subFolder, filename, true);
                }
                continue;
            }
            if (assetFunction)
            {
                int first = current.Text == "playMusic" ? 1 : 0;
                int last = current.Text is "loadFullShaderWithGeo" or "loadGeoShader" ? 3 : first + 1;
                bool texture = current.Text is "loadBlock" or "loadCharacter" or "loadSystem"
                    or "loadTileset" or "loadAutotile" or "loadFog" or "loadTransition";
                for (int argument = first; argument < Math.Min(last, arguments.Count); argument++)
                {
                    foreach (string filename in arguments[argument])
                        addAssetReference(references, folder!, filename, texture);
                }
                continue;
            }
            IEnumerable<string> values = arguments.Count == 0 ? [] : arguments[0];
            foreach (string value in values)
            {
                if (value.Length == 0)
                    continue;
                if (value.StartsWith("Data.", StringComparison.Ordinal)
                    || value.StartsWith("Data/", StringComparison.Ordinal)
                    || value.StartsWith("./Data/", StringComparison.Ordinal))
                    addExplicitReference(references, value);
                else
                {
                    string key = value.EndsWith(".json", StringComparison.OrdinalIgnoreCase)
                        ? value[..^5] : value;
                    references.Add($"Data/{section}/{key}.json");
                }
            }
        }
        return references;
    }

    private static bool isEmitterConstructor(IReadOnlyList<Token> tokens, int index)
    {
        return tokens[index].Text == "new" && index >= 2 && tokens[index - 1].Text == "."
            && tokens[index - 2].Kind == TokenKind.Identifier
            && tokens[index - 2].Text is "Emitter" or "EmitterView";
    }

    private static void addAssetReference(ISet<string> references, string folder, string filename, bool texture)
    {
        if (filename.Length == 0)
            return;
        if (texture)
        {
            int separator = filename.LastIndexOf('/');
            int extension = filename.LastIndexOf('.');
            if (extension < 0 || extension < separator)
                filename += ".png";
            else if (extension == filename.Length - 1)
                filename += "png";
        }
        if (GameAssetPath.TryGetRelativePath(filename, out string canonical))
            references.Add("Assets/" + canonical);
        else if (!Path.IsPathRooted(filename))
            references.Add($"Assets/{folder}/{filename}");
    }

    private static IReadOnlyList<IReadOnlyList<string>> readArguments(
        IReadOnlyList<Token> tokens, int opening, IReadOnlyDictionary<string, HashSet<string>> constants)
    {
        List<IReadOnlyList<string>> arguments = [];
        int start = opening + 1;
        int depth = 0;
        for (int index = start; index < tokens.Count; index++)
        {
            string symbol = tokens[index].Text;
            if (tokens[index].Kind == TokenKind.Symbol && depth == 0 && symbol is "," or ")")
            {
                int position = start;
                IReadOnlyList<string> values = readExpressionValues(tokens, ref position, index, constants);
                arguments.Add(position == index ? values : []);
                if (symbol == ")")
                    return arguments;
                start = index + 1;
            }
            else if (tokens[index].Kind == TokenKind.Symbol)
            {
                if (symbol is "(" or "{" or "[")
                    depth++;
                else if (symbol is ")" or "}" or "]")
                    depth--;
            }
        }
        throw new InvalidDataException("Unterminated Lua function arguments.");
    }

    private static IReadOnlyList<string> readExpressionValues(
        IReadOnlyList<Token> tokens, ref int position, int end,
        IReadOnlyDictionary<string, HashSet<string>> constants, int minimumPrecedence = 1)
    {
        if (position >= end)
            return [];
        Token first = tokens[position++];
        IReadOnlyList<string> values = [];
        if (first.Kind == TokenKind.String)
            values = [first.Text];
        else if (first.Kind == TokenKind.Symbol && first.Text == "(")
        {
            values = readExpressionValues(tokens, ref position, end, constants);
            if (position >= end || tokens[position].Kind != TokenKind.Symbol || tokens[position++].Text != ")")
                return [];
        }
        else if (first.Kind == TokenKind.Identifier && first.Text == "not"
            || first.Kind == TokenKind.Symbol && first.Text is "-" or "#" or "~")
        {
            readExpressionValues(tokens, ref position, end, constants, 11);
        }
        else if (first.Kind == TokenKind.Identifier)
        {
            if (first.Text is "function" or "if" or "then" or "end" or "else" or "elseif" or "local" or "return")
                return [];
            StringBuilder name = new(first.Text);
            while (position + 1 < end && tokens[position].Kind == TokenKind.Symbol
                && tokens[position].Text == "." && tokens[position + 1].Kind == TokenKind.Identifier)
            {
                name.Append('.').Append(tokens[position + 1].Text);
                position += 2;
            }
            string key = name.ToString();
            if (constants.TryGetValue(key, out HashSet<string>? known))
                values = known.ToArray();
            else if (key.StartsWith("self.", StringComparison.Ordinal))
            {
                string suffix = key["self".Length..];
                values = constants.Where(pair => pair.Key.EndsWith(suffix, StringComparison.Ordinal))
                    .SelectMany(pair => pair.Value).Distinct(StringComparer.Ordinal).ToArray();
            }
        }
        else if (first.Kind == TokenKind.Symbol && first.Text == "{")
        {
            skipExpressionGroup(tokens, ref position, end);
        }

        while (position < end)
        {
            Token next = tokens[position];
            if (next.Kind == TokenKind.Symbol && next.Text is "(" or "[" or "{")
            {
                position++;
                skipExpressionGroup(tokens, ref position, end);
                values = [];
            }
            else if (next.Kind == TokenKind.Symbol && next.Text is "." or ":"
                && position + 1 < end && tokens[position + 1].Kind == TokenKind.Identifier)
            {
                position += 2;
                values = [];
            }
            else if (next.Kind == TokenKind.String)
            {
                position++;
                values = [];
            }
            else
                break;
        }

        while (position < end)
        {
            (string operation, int width, int precedence) = readBinaryOperator(tokens, position, end);
            if (precedence < minimumPrecedence)
                break;
            position += width;
            IReadOnlyList<string> right = readExpressionValues(tokens, ref position, end, constants,
                operation is ".." or "^" ? precedence : precedence + 1);
            values = operation is "and" or "or"
                ? values.Concat(right).Distinct(StringComparer.Ordinal).ToArray() : [];
        }
        return values;
    }

    private static void skipExpressionGroup(IReadOnlyList<Token> tokens, ref int position, int end)
    {
        int depth = 1;
        while (position < end && depth != 0)
        {
            Token token = tokens[position++];
            if (token.Kind != TokenKind.Symbol)
                continue;
            if (token.Text is "(" or "[" or "{")
                depth++;
            else if (token.Text is ")" or "]" or "}")
                depth--;
        }
    }

    private static (string Operation, int Width, int Precedence) readBinaryOperator(
        IReadOnlyList<Token> tokens, int position, int end)
    {
        Token token = tokens[position];
        string operation = token.Text;
        int width = 1;
        if (token.Kind == TokenKind.Symbol && position + 1 < end && tokens[position + 1].Kind == TokenKind.Symbol)
        {
            string combined = operation + tokens[position + 1].Text;
            if (combined is "==" or "~=" or "<=" or ">=" or "<<" or ">>" or "//")
            {
                operation = combined;
                width = 2;
            }
        }
        int precedence = token.Kind == TokenKind.Identifier
            ? operation switch { "or" => 1, "and" => 2, _ => 0 }
            : token.Kind == TokenKind.Symbol ? operation switch
            {
                "==" or "~=" or "<" or ">" or "<=" or ">=" => 3,
                "|" => 4, "~" => 5, "&" => 6, "<<" or ">>" => 7, ".." => 8,
                "+" or "-" => 9, "*" or "/" or "//" or "%" => 10, "^" => 12, _ => 0,
            } : 0;
        return (operation, width, precedence);
    }

    public static string? ExplicitReference(string value)
    {
        if (GameAssetPath.TryGetRelativePath(value, out string assetPath))
            return "Assets/" + assetPath;
        string normalized = value.StartsWith("./", StringComparison.Ordinal) ? value[2..] : value;
        if (normalized.StartsWith("Assets/", StringComparison.Ordinal)
            && GameAssetPath.TryGetRelativePath("/Game/" + normalized, out string relativeAsset))
            return "Assets/" + relativeAsset;
        if (normalized.StartsWith("Data/", StringComparison.Ordinal)
            && normalized.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
            return normalized;
        if (normalized.StartsWith("Data.", StringComparison.Ordinal))
        {
            string[] parts = normalized.Split('.');
            if (parts.Length >= 3 && parts.All(part => part.Length != 0))
                return string.Join('/', parts) + ".json";
        }
        return null;
    }

    private static void addExplicitReference(ISet<string> references, string value)
    {
        if (ExplicitReference(value) is string path)
            references.Add(path);
    }

    private static string precedingName(IReadOnlyList<Token> tokens, int end)
    {
        if (tokens[end].Kind != TokenKind.Identifier)
            return string.Empty;
        int start = end;
        while (start >= 2 && tokens[start - 1].Text == "."
            && tokens[start - 2].Kind == TokenKind.Identifier)
            start -= 2;
        return string.Concat(tokens.Skip(start).Take(end - start + 1).Select(item => item.Text));
    }

    private static IReadOnlyList<string?> readAssignmentNames(IReadOnlyList<Token> tokens, int assignment)
    {
        List<string?> names = [];
        int position = assignment - 1;
        while (position >= 0)
        {
            if (position >= 2 && tokens[position].Text == ">"
                && tokens[position - 1].Kind == TokenKind.Identifier
                && tokens[position - 1].Text is "const" or "close"
                && tokens[position - 2].Text == "<")
                position -= 3;
            if (position < 0 || tokens[position].Kind != TokenKind.Identifier
                && !(tokens[position].Kind == TokenKind.Symbol && tokens[position].Text == "]"))
                break;
            int end = position;
            bool simple = true;
            if (!readAssignmentPrefix(tokens, ref position, ref simple))
                break;
            if (position >= 0 && (tokens[position].Text is "=" or "not"
                || readBinaryOperator(tokens, position, assignment).Precedence != 0))
                break;
            names.Add(simple
                ? string.Concat(tokens.Skip(position + 1).Take(end - position).Select(item => item.Text))
                : null);
            if (position < 0 || tokens[position].Kind != TokenKind.Symbol || tokens[position].Text != ",")
                break;
            position--;
        }
        names.Reverse();
        return names;
    }

    private static bool readAssignmentPrefix(IReadOnlyList<Token> tokens, ref int position, ref bool simple)
    {
        if (position < 0)
            return false;
        Token current = tokens[position];
        if (current.Kind == TokenKind.Identifier)
        {
            if (current.Text is "and" or "break" or "do" or "else" or "elseif" or "end" or "false"
                or "for" or "function" or "goto" or "if" or "in" or "local" or "nil" or "not"
                or "or" or "repeat" or "return" or "then" or "true" or "until" or "while")
                return false;
            position--;
            if (position >= 0 && tokens[position].Kind == TokenKind.Symbol && tokens[position].Text is "." or ":")
            {
                simple &= tokens[position].Text == ".";
                position--;
                return readAssignmentPrefix(tokens, ref position, ref simple);
            }
            return true;
        }
        simple = false;
        if (current.Kind == TokenKind.String)
        {
            position--;
            return readAssignmentPrefix(tokens, ref position, ref simple);
        }
        if (current.Kind != TokenKind.Symbol || current.Text is not "]" and not ")" and not "}")
            return false;
        int depth = 1;
        position--;
        while (position >= 0 && depth != 0)
        {
            Token previous = tokens[position--];
            if (previous.Kind != TokenKind.Symbol)
                continue;
            if (previous.Text is "]" or ")" or "}")
                depth++;
            else if (previous.Text is "[" or "(" or "{")
                depth--;
        }
        if (depth != 0)
            return false;
        if (current.Text != ")")
            return readAssignmentPrefix(tokens, ref position, ref simple);
        int beforeGroup = position;
        if (!readAssignmentPrefix(tokens, ref position, ref simple))
            position = beforeGroup;
        return true;
    }

    private static List<Token> tokenize(string source, CancellationToken token)
    {
        List<Token> result = [];
        Stack<char> brackets = new();
        for (int position = 0; position < source.Length;)
        {
            token.ThrowIfCancellationRequested();
            char current = source[position];
            if (char.IsWhiteSpace(current) || current == '\uFEFF')
            {
                position++;
                continue;
            }
            if (position + 1 < source.Length && current == '-' && source[position + 1] == '-')
            {
                position += 2;
                if (tryLongString(source, ref position, out _))
                    continue;
                while (position < source.Length && source[position] is not '\r' and not '\n')
                    position++;
                continue;
            }
            if (current is '\'' or '"')
            {
                result.Add(new Token(TokenKind.String, shortString(source, ref position)));
                continue;
            }
            if (tryLongString(source, ref position, out string? longString))
            {
                result.Add(new Token(TokenKind.String, longString!));
                continue;
            }
            if (char.IsAsciiDigit(current)
                || current == '.' && position + 1 < source.Length && char.IsAsciiDigit(source[position + 1]))
            {
                int start = position++;
                while (position < source.Length)
                {
                    char character = source[position];
                    if (character == '.' && position + 1 < source.Length && source[position + 1] == '.')
                        break;
                    if (!char.IsAsciiLetterOrDigit(character) && character != '.'
                        && !(character is '+' or '-' && source[position - 1] is 'e' or 'E' or 'p' or 'P'))
                        break;
                    position++;
                }
                result.Add(new Token(TokenKind.Number, source[start..position]));
                continue;
            }
            if (char.IsLetter(current) || current == '_')
            {
                int start = position++;
                while (position < source.Length && (char.IsLetterOrDigit(source[position]) || source[position] == '_'))
                    position++;
                result.Add(new Token(TokenKind.Identifier, source[start..position]));
                continue;
            }
            if (current is '(' or '{' or '[')
                brackets.Push(current);
            else if (current is ')' or '}' or ']')
            {
                char expected = current switch { ')' => '(', '}' => '{', _ => '[' };
                if (!brackets.TryPop(out char opened) || opened != expected)
                    throw new InvalidDataException("Unmatched Lua delimiter.");
            }
            if (position + 1 < source.Length && current == '.' && source[position + 1] == '.')
            {
                result.Add(new Token(TokenKind.Symbol, ".."));
                position += 2;
            }
            else
            {
                result.Add(new Token(TokenKind.Symbol, current.ToString()));
                position++;
            }
        }
        if (brackets.Count != 0)
            throw new InvalidDataException("Unterminated Lua delimiter.");
        return result;
    }

    private static bool tryLongString(string source, ref int position, out string? value)
    {
        value = null;
        if (position >= source.Length || source[position] != '[')
            return false;
        int opening = position + 1;
        while (opening < source.Length && source[opening] == '=')
            opening++;
        if (opening == source.Length || source[opening] != '[')
            return false;
        string closing = "]" + new string('=', opening - position - 1) + "]";
        int end = source.IndexOf(closing, opening + 1, StringComparison.Ordinal);
        if (end < 0)
            throw new InvalidDataException("Unterminated Lua long string or comment.");
        int start = opening + 1;
        if (start < end && source[start] == '\r')
            start++;
        if (start < end && source[start] == '\n')
            start++;
        value = source[start..end].Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n');
        position = end + closing.Length;
        return true;
    }

    private static string shortString(string source, ref int position)
    {
        char quote = source[position++];
        StringBuilder value = new();
        List<byte> bytes = [];
        while (position < source.Length)
        {
            char current = source[position++];
            if (current == quote)
            {
                if (bytes.Count == 0)
                    return value.ToString();
                bytes.AddRange(Encoding.UTF8.GetBytes(value.ToString()));
                return Encoding.UTF8.GetString(bytes.ToArray());
            }
            if (current is '\n' or '\r')
                throw new InvalidDataException("Unterminated Lua string.");
            if (current != '\\')
            {
                value.Append(current);
                continue;
            }
            if (position == source.Length)
                break;
            char escaped = source[position++];
            if (escaped == 'z')
            {
                while (position < source.Length && char.IsWhiteSpace(source[position]))
                    position++;
            }
            else if (escaped == 'x')
            {
                if (position + 2 > source.Length || !byte.TryParse(source.AsSpan(position, 2), NumberStyles.HexNumber,
                        CultureInfo.InvariantCulture, out byte number))
                    throw new InvalidDataException("Invalid hexadecimal Lua escape.");
                appendByte(value, bytes, number);
                position += 2;
            }
            else if (escaped == 'u')
            {
                int end = source.IndexOf('}', position);
                if (position >= source.Length || source[position] != '{' || end < 0
                    || !int.TryParse(source.AsSpan(position + 1, end - position - 1), NumberStyles.HexNumber,
                        CultureInfo.InvariantCulture, out int number) || !Rune.IsValid(number))
                    throw new InvalidDataException("Invalid Unicode Lua escape.");
                value.Append(char.ConvertFromUtf32(number));
                position = end + 1;
            }
            else if (escaped is >= '0' and <= '9')
            {
                int number = escaped - '0';
                for (int count = 1; count < 3 && position < source.Length && source[position] is >= '0' and <= '9'; count++)
                    number = number * 10 + source[position++] - '0';
                if (number > 255)
                    throw new InvalidDataException("Lua decimal escape exceeds one byte.");
                appendByte(value, bytes, (byte)number);
            }
            else
            {
                value.Append(escaped switch
                {
                    'a' => '\a', 'b' => '\b', 'f' => '\f', 'n' => '\n', 'r' => '\r', 't' => '\t', 'v' => '\v',
                    '\\' => '\\', '\'' => '\'', '"' => '"', '\n' => '\n', '\r' => '\n',
                    _ => throw new InvalidDataException($"Invalid Lua escape: {escaped}"),
                });
                if (escaped == '\r' && position < source.Length && source[position] == '\n')
                    position++;
            }
        }
        throw new InvalidDataException("Unterminated Lua string.");
    }

    private static void appendByte(StringBuilder text, List<byte> bytes, byte value)
    {
        bytes.AddRange(Encoding.UTF8.GetBytes(text.ToString()));
        text.Clear();
        bytes.Add(value);
    }

    private enum TokenKind { Identifier, String, Number, Symbol }
    private sealed record Token(TokenKind Kind, string Text);
}
