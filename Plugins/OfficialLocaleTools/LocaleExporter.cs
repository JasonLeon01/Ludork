using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace Ludork.Plugins.OfficialLocaleTools;

internal sealed record DuplicateLocaleId(
    string Id,
    string FirstLocation,
    string DuplicateLocation);

internal sealed record LocaleExportResult(
    int LanguageCount,
    int EntryCount,
    IReadOnlyList<DuplicateLocaleId> Duplicates);

internal sealed class LocaleWorkbookData
{
    public LocaleWorkbookData(
        IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>> languages,
        IReadOnlyList<DuplicateLocaleId> duplicates)
    {
        Languages = languages;
        Duplicates = duplicates;
    }

    public IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>> Languages { get; }
    public IReadOnlyList<DuplicateLocaleId> Duplicates { get; }
}

internal static class LocaleExporter
{
    private sealed record PendingLocaleFile(string TemporaryPath, string DestinationPath);

    public static LocaleExportResult Export(
        string workbookPath,
        string outputDirectory,
        CancellationToken cancellationToken)
    {
        LocaleWorkbookData workbook = XlsxLocaleWorkbookReader.Read(workbookPath, cancellationToken);
        Directory.CreateDirectory(outputDirectory);

        List<PendingLocaleFile> pendingFiles = new List<PendingLocaleFile>();
        try
        {
            foreach (KeyValuePair<string, IReadOnlyDictionary<string, string>> languageEntry in
                     workbook.Languages.OrderBy(entry => entry.Key, StringComparer.Ordinal))
            {
                cancellationToken.ThrowIfCancellationRequested();
                ValidateLanguageName(languageEntry.Key);
                string destinationPath = Path.Combine(outputDirectory, languageEntry.Key + ".lua");
                string temporaryPath = Path.Combine(
                    outputDirectory,
                    "." + languageEntry.Key + "." + Guid.NewGuid().ToString("N") + ".tmp");
                PendingLocaleFile pendingFile = new PendingLocaleFile(temporaryPath, destinationPath);
                pendingFiles.Add(pendingFile);
                WriteUtf8File(temporaryPath, BuildLuaTable(languageEntry.Value));
            }

            foreach (PendingLocaleFile pendingFile in pendingFiles)
            {
                cancellationToken.ThrowIfCancellationRequested();
                File.Move(pendingFile.TemporaryPath, pendingFile.DestinationPath, true);
            }
        }
        finally
        {
            DeleteTemporaryFiles(pendingFiles);
        }

        int entryCount = workbook.Languages.Sum(language => language.Value.Count);
        return new LocaleExportResult(
            workbook.Languages.Count,
            entryCount,
            workbook.Duplicates);
    }

    private static void ValidateLanguageName(string language)
    {
        char[] invalidCharacters = Path.GetInvalidFileNameChars();
        if (string.IsNullOrWhiteSpace(language) ||
            language is "." or ".." ||
            string.Equals(language, "Core", StringComparison.OrdinalIgnoreCase) ||
            language.IndexOf('/') >= 0 ||
            language.IndexOf('\\') >= 0 ||
            language.IndexOfAny(invalidCharacters) >= 0 ||
            language.IndexOfAny(new[] { '<', '>', ':', '"', '|', '?', '*' }) >= 0)
        {
            throw new InvalidDataException("Invalid locale language name: " + language);
        }
    }

    private static string BuildLuaTable(IReadOnlyDictionary<string, string> mapping)
    {
        StringBuilder builder = new StringBuilder();
        builder.Append("return {\n");
        foreach (KeyValuePair<string, string> entry in
                 mapping.OrderBy(entry => entry.Key, StringComparer.Ordinal))
        {
            builder.Append("    [");
            AppendLuaString(builder, entry.Key);
            builder.Append("] = ");
            AppendLuaString(builder, entry.Value);
            builder.Append(",\n");
        }
        builder.Append("}\n");
        return builder.ToString();
    }

    private static void AppendLuaString(StringBuilder builder, string value)
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
                    {
                        builder.Append('\\');
                        builder.Append(((int)character).ToString("D3"));
                    }
                    else
                    {
                        builder.Append(character);
                    }
                    break;
            }
        }
        builder.Append('"');
    }

    private static void WriteUtf8File(string path, string content)
    {
        UTF8Encoding encoding = new UTF8Encoding(false);
        byte[] bytes = encoding.GetBytes(content);
        using FileStream stream = new FileStream(
            path,
            FileMode.CreateNew,
            FileAccess.Write,
            FileShare.None);
        stream.Write(bytes, 0, bytes.Length);
        stream.Flush(true);
    }

    private static void DeleteTemporaryFiles(IEnumerable<PendingLocaleFile> pendingFiles)
    {
        foreach (PendingLocaleFile pendingFile in pendingFiles)
        {
            try
            {
                if (File.Exists(pendingFile.TemporaryPath))
                {
                    File.Delete(pendingFile.TemporaryPath);
                }
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }
}
