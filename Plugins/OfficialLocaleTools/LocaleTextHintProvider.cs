using System;
using System.Collections.Generic;
using System.IO;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialLocaleTools;

internal sealed class LocaleTextHintProvider : ITextHintProvider
{
    private readonly object _sync = new object();
    private string? _cachedPath;
    private DateTime _cachedWriteTimeUtc;
    private long _cachedLength;
    private IReadOnlyDictionary<string, string>? _cachedValues;

    public string? Resolve(TextHintContext context)
    {
        string? localeId = ExtractLocaleId(context.Text);
        if (localeId is null || string.IsNullOrWhiteSpace(context.ProjectPath))
        {
            return null;
        }

        string localeDirectory =
            LocaleProjectPaths.GetOutputDirectory(context.ProjectPath);
        string language = SafeLanguageName(context.EditorLanguage)
            ? context.EditorLanguage
            : "en_GB";
        string selectedPath = Path.Combine(localeDirectory, language + ".lua");
        if (!File.Exists(selectedPath) &&
            !string.Equals(language, "en_GB", StringComparison.Ordinal))
        {
            selectedPath = Path.Combine(localeDirectory, "en_GB.lua");
        }
        if (!File.Exists(selectedPath))
        {
            return null;
        }

        IReadOnlyDictionary<string, string>? values = LoadLocaleFile(selectedPath);
        if (values is not null &&
            values.TryGetValue(localeId, out string? value))
        {
            return value;
        }
        return null;
    }

    public void Invalidate()
    {
        lock (_sync)
        {
            _cachedPath = null;
            _cachedWriteTimeUtc = default;
            _cachedLength = 0;
            _cachedValues = null;
        }
    }

    private IReadOnlyDictionary<string, string>? LoadLocaleFile(string path)
    {
        try
        {
            FileInfo fileInfo = new FileInfo(path);
            DateTime writeTimeUtc = fileInfo.LastWriteTimeUtc;
            long length = fileInfo.Length;
            lock (_sync)
            {
                if (string.Equals(_cachedPath, path, StringComparison.Ordinal) &&
                    _cachedWriteTimeUtc == writeTimeUtc &&
                    _cachedLength == length &&
                    _cachedValues is not null)
                {
                    return _cachedValues;
                }

                IReadOnlyDictionary<string, string> values = LuaLocaleTableReader.Read(path);
                _cachedPath = path;
                _cachedWriteTimeUtc = writeTimeUtc;
                _cachedLength = length;
                _cachedValues = values;
                return values;
            }
        }
        catch (IOException)
        {
            return null;
        }
        catch (UnauthorizedAccessException)
        {
            return null;
        }
        catch (FormatException)
        {
            return null;
        }
    }

    private static string? ExtractLocaleId(string text)
    {
        string token = text.Trim();
        if (token.Length < 3 ||
            token[0] != '{' ||
            token[^1] != '}')
        {
            return null;
        }

        string localeId = token[1..^1];
        foreach (char character in localeId)
        {
            if (char.IsWhiteSpace(character) ||
                character is '{' or '}')
            {
                return null;
            }
        }
        return localeId.Length == 0 ? null : localeId;
    }

    private static bool SafeLanguageName(string language)
    {
        if (string.IsNullOrWhiteSpace(language) ||
            language is "." or ".." ||
            language.IndexOf('/') >= 0 ||
            language.IndexOf('\\') >= 0)
        {
            return false;
        }
        return language.IndexOfAny(Path.GetInvalidFileNameChars()) < 0;
    }
}
