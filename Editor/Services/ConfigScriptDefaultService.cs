using Ludork.Models;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using Token = Ludork.Services.LuaSourceTokens.Token;
using TokenKind = Ludork.Services.LuaSourceTokens.TokenKind;

namespace Ludork.Services;

public static class ConfigScriptDefaultService
{
    public static JsonNode Read(string projectPath, LuaEnumService enums, JsonObject field)
    {
        if (field["scriptDefault"] is not JsonObject reference
            || reference["module"] is not JsonValue moduleValue || !moduleValue.TryGetValue(out string? module)
            || reference["field"] is not JsonValue fieldValue || !fieldValue.TryGetValue(out string? member)
            || !isPath(module) || !isPath(member) || member.Split('.').Length != 2)
        {
            throw new InvalidDataException("scriptDefault requires a Lua module and a table.field reference.");
        }
        string path = Path.Combine(projectPath, "Scripts", module.Replace('.', Path.DirectorySeparatorChar) + ".lua");
        string source = File.ReadAllText(path);
        List<Token> tokens = LuaSourceTokens.Read(source);
        List<int> topLevel = topLevelTokens(tokens, out List<int> outsideFunctions);
        string[] parts = member.Split('.');
        int[] assignments = outsideFunctions.Where(index => isFieldAssignment(tokens, index, parts)).ToArray();
        if (assignments.Any(index => !topLevel.Contains(index))
            || outsideFunctions.Any(index => matches(tokens, index, "function", parts[0], ".", parts[1])))
            throw new InvalidDataException($"{module}: {member} requires an unconditional scalar assignment.");
        if (assignments.Length != 1)
            throw new InvalidDataException($"{module}: expected one top-level assignment to {member}.");
        int assignment = assignments[0];
        if (!matches(tokens, assignment, parts[0], ".", parts[1], "=")
            || matches(tokens, assignment - 1, ","))
            throw new InvalidDataException($"{module}: {member} requires a single table.field assignment.");
        Dictionary<string, (string Module, int Index)> imports = new(StringComparer.Ordinal);
        foreach (int index in topLevel.Where(index => index < assignment))
        {
            if (matches(tokens, index, "local") && index + 5 < tokens.Count
                && tokens[index + 1].Kind == TokenKind.Identifier
                && matches(tokens, index + 2, "=", "require", "(")
                && tokens[index + 5].Kind == TokenKind.String
                && matches(tokens, index + 6, ")"))
            {
                imports.TryAdd(tokens[index + 1].Text, (tokens[index + 5].Text, index));
            }
        }
        int position = assignment + 4;
        DynValue result = readExpression(tokens, ref position, enums, imports, outsideFunctions);
        if (position < tokens.Count && !matches(tokens, position, ";")
            && (tokens[position].Kind != TokenKind.Identifier || tokens[position].Text is "and" or "or"
                || !source.AsSpan(tokens[position - 1].End, tokens[position].Start - tokens[position - 1].End).ContainsAny('\r', '\n')))
        {
            throw new InvalidDataException($"{module}: {member} must use a scalar constant expression.");
        }
        string? type = field["type"]?.GetValue<string>();
        JsonNode value = (type, result.Type) switch
        {
            ("bool", DataType.Boolean) => JsonValue.Create(result.Boolean)!,
            ("string", DataType.String) => JsonValue.Create(result.String)!,
            ("int", DataType.Number) when double.IsFinite(result.Number)
                && result.Number == Math.Truncate(result.Number)
                && result.Number >= int.MinValue && result.Number <= int.MaxValue => JsonValue.Create((int)result.Number)!,
            ("float", DataType.Number) when double.IsFinite(result.Number)
                && result.Number >= -999999999 && result.Number <= 999999999 => JsonValue.Create(result.Number)!,
            _ => throw new InvalidDataException($"{module}: {member} is not a supported {type} value."),
        };
        if (field["enum"]?.GetValue<string>() is { Length: > 0 } enumModule)
        {
            LuaEnumDefinition definition = enums.Read(enumModule);
            if (definition.Error is not null)
                throw new InvalidDataException(definition.Error);
            if (!definition.Options.Any(option => JsonNode.DeepEquals(option.Value, value)))
                throw new InvalidDataException($"{module}: {member} is not a member of {enumModule}.");
        }
        return value;
    }

    private static DynValue readExpression(
        IReadOnlyList<Token> tokens, ref int position, LuaEnumService enums,
        IReadOnlyDictionary<string, (string Module, int Index)> imports, IReadOnlyList<int> outsideFunctions)
    {
        Script script = new(CoreModules.None);
        int cursor = position;
        int literalIndex = 0;
        string literal(DynValue value)
        {
            string name = "value" + literalIndex++;
            script.Globals.Set(name, value);
            return name;
        }
        string atom()
        {
            if (cursor >= tokens.Count)
                throw new InvalidDataException("Missing script default expression.");
            Token token = tokens[cursor++];
            if (token.Text is "-" or "not" && token.Kind != TokenKind.String)
                return token.Text + " " + atom();
            if (token.Kind == TokenKind.Symbol && token.Text == "(")
            {
                string inner = expression();
                if (!matches(tokens, cursor++, ")"))
                    throw new InvalidDataException("Unclosed script default expression.");
                return "(" + inner + ")";
            }
            if (token.Kind == TokenKind.String)
                return literal(DynValue.NewString(token.Text));
            if (token.Kind == TokenKind.Number)
                return token.Text;
            if (token.Kind == TokenKind.Identifier && token.Text is "true" or "false")
                return token.Text;
            if (token.Kind == TokenKind.Identifier && imports.TryGetValue(token.Text, out (string Module, int Index) import)
                && matches(tokens, cursor, ".") && cursor + 1 < tokens.Count
                && tokens[cursor + 1].Kind == TokenKind.Identifier)
            {
                if (outsideFunctions.Any(index => index > import.Index + 6 && index < cursor - 1
                    && isAliasMutation(tokens, index, token.Text)))
                {
                    throw new InvalidDataException($"Enum import {token.Text} is reassigned before its script default is read.");
                }
                string module = import.Module;
                string name = tokens[cursor + 1].Text;
                cursor += 2;
                LuaEnumDefinition definition = enums.Read(module);
                if (definition.Error is not null)
                    throw new InvalidDataException(definition.Error);
                BlueprintVariableOption? option = definition.Options.FirstOrDefault(option => option.Label == name);
                if (option?.Value is not JsonValue constant)
                    throw new InvalidDataException($"Missing enum constant {module}.{name}.");
                return literal(constant.TryGetValue(out bool boolean) ? DynValue.NewBoolean(boolean)
                    : constant.TryGetValue(out string? text) ? DynValue.NewString(text)
                    : DynValue.NewNumber(double.Parse(constant.ToJsonString(), System.Globalization.CultureInfo.InvariantCulture)));
            }
            throw new InvalidDataException("Script defaults support scalar literals, enum members and scalar arithmetic only.");
        }
        string expression()
        {
            string result = atom();
            while (cursor < tokens.Count && tokens[cursor].Kind == TokenKind.Symbol
                && tokens[cursor].Text is "+" or "-" or "*" or "/" or "%" or "^" or "..")
            {
                string operation = tokens[cursor++].Text;
                result += " " + operation + " " + atom();
            }
            return result;
        }
        string expressionText = expression();
        position = cursor;
        return script.DoString("return " + expressionText);
    }

    private static List<int> topLevelTokens(IReadOnlyList<Token> tokens, out List<int> outsideFunctions)
    {
        List<int> result = [];
        outsideFunctions = [];
        Stack<bool> blocks = new();
        int functions = 0;
        int brackets = 0;
        for (int index = 0; index < tokens.Count; index++)
        {
            Token token = tokens[index];
            if (blocks.Count == 0 && brackets == 0)
                result.Add(index);
            if (functions == 0 && brackets == 0)
                outsideFunctions.Add(index);
            if (token.Kind == TokenKind.Identifier)
            {
                if (token.Text is "function" or "if" or "do" or "repeat")
                {
                    bool function = token.Text == "function";
                    blocks.Push(function);
                    if (function)
                        functions++;
                }
                else if (token.Text is "end" or "until")
                {
                    if (!blocks.TryPop(out bool function))
                        throw new InvalidDataException("Unmatched Lua block.");
                    if (function)
                        functions--;
                }
            }
            else if (token.Kind == TokenKind.Symbol)
            {
                if (token.Text is "(" or "[" or "{")
                    brackets++;
                else if (token.Text is ")" or "]" or "}")
                    brackets--;
            }
        }
        if (blocks.Count != 0)
            throw new InvalidDataException("Unterminated Lua block.");
        return result;
    }

    private static bool isFieldAssignment(IReadOnlyList<Token> tokens, int index, string[] parts)
    {
        if (index > 0 && tokens[index - 1].Kind == TokenKind.Symbol && tokens[index - 1].Text is "." or ":")
            return false;
        if (matches(tokens, index, parts[0], ".", parts[1]))
            return hasAssignmentTail(tokens, index + 3);
        return matches(tokens, index, parts[0], "[") && index + 4 < tokens.Count
            && tokens[index + 2].Kind == TokenKind.String && tokens[index + 2].Text == parts[1]
            && matches(tokens, index + 3, "]") && hasAssignmentTail(tokens, index + 4);
    }

    private static bool isAliasMutation(IReadOnlyList<Token> tokens, int index, string alias)
    {
        if (matches(tokens, index, "function", alias)
            || matches(tokens, index, "local", "function", alias))
            return true;
        if (matches(tokens, index, "local"))
        {
            for (int name = index + 1; name < tokens.Count && tokens[name].Kind == TokenKind.Identifier; name += 2)
            {
                if (tokens[name].Text == alias)
                    return true;
                if (!matches(tokens, name + 1, ","))
                    break;
            }
            return false;
        }
        if (!matches(tokens, index, alias) || index > 0
            && tokens[index - 1].Kind == TokenKind.Symbol && tokens[index - 1].Text is "." or ":")
            return false;
        int end = targetEnd(tokens, index);
        return end > index && hasAssignmentTail(tokens, end);
    }

    private static bool hasAssignmentTail(IReadOnlyList<Token> tokens, int position)
    {
        while (matches(tokens, position, ","))
        {
            int end = targetEnd(tokens, position + 1);
            if (end == position + 1)
                return false;
            position = end;
        }
        return matches(tokens, position, "=") && !matches(tokens, position + 1, "=");
    }

    private static int targetEnd(IReadOnlyList<Token> tokens, int position)
    {
        if (position >= tokens.Count || tokens[position].Kind != TokenKind.Identifier)
            return position;
        int end = position + 1;
        while (end < tokens.Count)
        {
            if (matches(tokens, end, ".") && end + 1 < tokens.Count && tokens[end + 1].Kind == TokenKind.Identifier)
            {
                end += 2;
            }
            else if (matches(tokens, end, "["))
            {
                int depth = 1;
                while (++end < tokens.Count && depth != 0)
                {
                    if (matches(tokens, end, "["))
                        depth++;
                    else if (matches(tokens, end, "]"))
                        depth--;
                }
            }
            else
                break;
        }
        return end;
    }

    private static bool matches(IReadOnlyList<Token> tokens, int start, params string[] expected)
    {
        return start >= 0 && start + expected.Length <= tokens.Count
            && expected.Select((text, offset) => tokens[start + offset].Kind != TokenKind.String
                && tokens[start + offset].Text == text).All(match => match);
    }

    private static bool isPath(string? value) => !string.IsNullOrEmpty(value)
        && value.Split('.').All(part => part.Length > 0 && (char.IsAsciiLetter(part[0]) || part[0] == '_')
            && part.All(character => char.IsAsciiLetterOrDigit(character) || character == '_'));
}
