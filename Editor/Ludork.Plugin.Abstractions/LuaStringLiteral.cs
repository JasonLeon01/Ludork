using System.Globalization;
using System.Text;

namespace Ludork.Plugin.Abstractions;

public static class LuaStringLiteral
{
    public static string Quote(string value)
    {
        StringBuilder builder = new(value.Length + 2);
        Append(builder, value);
        return builder.ToString();
    }

    public static void Append(StringBuilder builder, string value)
    {
        builder.Append('"');
        foreach (char character in value)
        {
            switch (character)
            {
                case '\\':
                    builder.Append("\\\\");
                    break;
                case '"':
                    builder.Append("\\\"");
                    break;
                case '\n':
                    builder.Append("\\n");
                    break;
                case '\r':
                    builder.Append("\\r");
                    break;
                case '\t':
                    builder.Append("\\t");
                    break;
                default:
                    if (character < 32 || character == 127)
                        builder.Append('\\').Append(((int)character).ToString("D3", CultureInfo.InvariantCulture));
                    else
                        builder.Append(character);
                    break;
            }
        }
        builder.Append('"');
    }
}
