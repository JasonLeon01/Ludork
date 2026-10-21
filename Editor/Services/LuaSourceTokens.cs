using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading;

namespace Ludork.Services;

internal static class LuaSourceTokens
{
    public static List<Token> Read(string source, CancellationToken token = default)
    {
        List<Token> result = [];
        Stack<char> brackets = new();
        for (int position = 0; position < source.Length;)
        {
            token.ThrowIfCancellationRequested();
            char current = source[position];
            int tokenStart = position;
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
                result.Add(new Token(TokenKind.String, shortString(source, ref position), tokenStart, position));
                continue;
            }
            if (tryLongString(source, ref position, out string? longString))
            {
                result.Add(new Token(TokenKind.String, longString!, tokenStart, position));
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
                result.Add(new Token(TokenKind.Number, source[start..position], tokenStart, position));
                continue;
            }
            if (char.IsLetter(current) || current == '_')
            {
                int start = position++;
                while (position < source.Length && (char.IsLetterOrDigit(source[position]) || source[position] == '_'))
                    position++;
                result.Add(new Token(TokenKind.Identifier, source[start..position], tokenStart, position));
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
                result.Add(new Token(TokenKind.Symbol, "..", tokenStart, position + 2));
                position += 2;
            }
            else
            {
                result.Add(new Token(TokenKind.Symbol, current.ToString(), tokenStart, position + 1));
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

    public enum TokenKind { Identifier, String, Number, Symbol }
    public sealed record Token(TokenKind Kind, string Text, int Start, int End);
}
