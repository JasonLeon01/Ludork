using System.IO;
using System.Text;

namespace Ludork.Services;

internal abstract class ConfigDocument
{
    private static readonly UTF8Encoding strictUtf8 = new(false, true);

    public static string ReadText(string path)
    {
        if (!ConfigFileNames.IsPropertyList(path))
            return File.ReadAllText(path, Encoding.UTF8);
        byte[] bytes = File.ReadAllBytes(path);
        int offset = bytes.Length >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
        try
        {
            return strictUtf8.GetString(bytes, offset, bytes.Length - offset);
        }
        catch (DecoderFallbackException exception)
        {
            throw new InvalidDataException("Configuration property lists must contain valid UTF-8 text.", exception);
        }
    }

    public static ConfigDocument Create(string path) => ConfigFileNames.IsPropertyList(path)
        ? new PlistConfigDocument()
        : IniConfigDocument.Parse(string.Empty);

    public static ConfigDocument Parse(string path, string text) => ConfigFileNames.IsPropertyList(path)
        ? PlistConfigDocument.Parse(text)
        : IniConfigDocument.Parse(text);

    public abstract string? GetValue(string section, string key);
    public abstract void SetValue(string section, string key, string value);
    public abstract string ToText();
}
