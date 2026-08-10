using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace Ludork.Plugins.OfficialLocaleTools;

internal static class LuaLocaleTableReader
{
    public static IReadOnlyDictionary<string, string> Read(string path)
    {
        using StreamReader reader = new StreamReader(
            path,
            new UTF8Encoding(false, true),
            true);
        string content = reader.ReadToEnd();
        Parser parser = new Parser(content);
        return parser.Parse();
    }

    private sealed class Parser
    {
        private readonly string _content;
        private int _position;

        public Parser(string content)
        {
            _content = content;
        }

        public IReadOnlyDictionary<string, string> Parse()
        {
            Dictionary<string, string> values =
                new Dictionary<string, string>(StringComparer.Ordinal);
            SkipWhitespace();
            ExpectKeyword("return");
            SkipWhitespace();
            Expect('{');

            while (true)
            {
                SkipWhitespace();
                if (TryConsume('}'))
                {
                    break;
                }

                Expect('[');
                SkipWhitespace();
                string key = ParseString();
                SkipWhitespace();
                Expect(']');
                SkipWhitespace();
                Expect('=');
                SkipWhitespace();
                string value = ParseString();
                values[key] = value;
                SkipWhitespace();
                if (TryConsume(',') || TryConsume(';'))
                {
                    continue;
                }
                if (Current != '}')
                {
                    throw Error("Expected ',', ';', or '}'.");
                }
            }

            SkipWhitespace();
            if (_position != _content.Length)
            {
                throw Error("Unexpected content after the locale table.");
            }
            return values;
        }

        private string ParseString()
        {
            char quote = Current;
            if (quote is not '"' and not '\'')
            {
                throw Error("Expected a Lua string.");
            }
            _position++;

            StringBuilder builder = new StringBuilder();
            while (_position < _content.Length)
            {
                char character = _content[_position++];
                if (character == quote)
                {
                    return builder.ToString();
                }
                if (character == '\r' || character == '\n')
                {
                    throw Error("Lua strings cannot contain raw line breaks.");
                }
                if (character != '\\')
                {
                    builder.Append(character);
                    continue;
                }
                if (_position >= _content.Length)
                {
                    throw Error("Unterminated Lua escape sequence.");
                }
                AppendEscape(builder);
            }
            throw Error("Unterminated Lua string.");
        }

        private void AppendEscape(StringBuilder builder)
        {
            char escaped = _content[_position++];
            switch (escaped)
            {
                case 'a':
                    builder.Append('\a');
                    return;
                case 'b':
                    builder.Append('\b');
                    return;
                case 'f':
                    builder.Append('\f');
                    return;
                case 'n':
                    builder.Append('\n');
                    return;
                case 'r':
                    builder.Append('\r');
                    return;
                case 't':
                    builder.Append('\t');
                    return;
                case 'v':
                    builder.Append('\v');
                    return;
                case '\\':
                    builder.Append('\\');
                    return;
                case '"':
                    builder.Append('"');
                    return;
                case '\'':
                    builder.Append('\'');
                    return;
                case 'x':
                    builder.Append((char)ReadHexadecimalByte());
                    return;
                case 'z':
                    SkipWhitespace();
                    return;
                case '\r':
                    if (Current == '\n')
                    {
                        _position++;
                    }
                    builder.Append('\n');
                    return;
                case '\n':
                    builder.Append('\n');
                    return;
                default:
                    if (char.IsAsciiDigit(escaped))
                    {
                        builder.Append((char)ReadDecimalByte(escaped));
                        return;
                    }
                    throw Error("Unsupported Lua escape sequence.");
            }
        }

        private int ReadHexadecimalByte()
        {
            if (_position + 2 > _content.Length)
            {
                throw Error("Incomplete hexadecimal Lua escape sequence.");
            }
            string digits = _content.Substring(_position, 2);
            if (!int.TryParse(
                    digits,
                    NumberStyles.AllowHexSpecifier,
                    CultureInfo.InvariantCulture,
                    out int value))
            {
                throw Error("Invalid hexadecimal Lua escape sequence.");
            }
            _position += 2;
            return value;
        }

        private int ReadDecimalByte(char firstDigit)
        {
            int value = firstDigit - '0';
            int digits = 1;
            while (digits < 3 &&
                   _position < _content.Length &&
                   char.IsAsciiDigit(_content[_position]))
            {
                value = value * 10 + (_content[_position] - '0');
                _position++;
                digits++;
            }
            if (value > 255)
            {
                throw Error("Decimal Lua escape sequence is outside the byte range.");
            }
            return value;
        }

        private void ExpectKeyword(string keyword)
        {
            if (_position + keyword.Length > _content.Length ||
                !string.Equals(
                    _content.Substring(_position, keyword.Length),
                    keyword,
                    StringComparison.Ordinal))
            {
                throw Error("Expected '" + keyword + "'.");
            }
            _position += keyword.Length;
            if (_position < _content.Length &&
                (char.IsLetterOrDigit(_content[_position]) ||
                 _content[_position] == '_'))
            {
                throw Error("Invalid Lua keyword boundary.");
            }
        }

        private void Expect(char expected)
        {
            if (!TryConsume(expected))
            {
                throw Error("Expected '" + expected + "'.");
            }
        }

        private bool TryConsume(char expected)
        {
            if (Current != expected)
            {
                return false;
            }
            _position++;
            return true;
        }

        private void SkipWhitespace()
        {
            while (_position < _content.Length &&
                   char.IsWhiteSpace(_content[_position]))
            {
                _position++;
            }
        }

        private char Current =>
            _position < _content.Length ? _content[_position] : '\0';

        private FormatException Error(string message)
        {
            return new FormatException(
                message + " Position: " + _position.ToString(CultureInfo.InvariantCulture));
        }
    }
}
