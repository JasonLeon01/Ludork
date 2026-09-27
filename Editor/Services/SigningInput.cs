using System;
using System.IO;
using System.Text;

namespace Ludork.Services;

internal static class SigningInput
{
    public static bool HasInvalidCharacters(string? value) =>
        value is not null && value.IndexOfAny(['\0', '\r', '\n']) >= 0;

    public static bool IsNonEmptySingleLine(string? value) =>
        !string.IsNullOrEmpty(value) && !HasInvalidCharacters(value);

    public static bool IsAbsolutePath(string path) =>
        !string.IsNullOrWhiteSpace(path)
        && !HasInvalidCharacters(path)
        && Path.IsPathFullyQualified(path);

    public static string NormalizePath(string path)
    {
        if (!IsAbsolutePath(path))
        {
            throw new InvalidDataException(
                "Signing material paths must be absolute and contain no NUL or line breaks.");
        }
        return Path.GetFullPath(path).Normalize(NormalizationForm.FormC);
    }

    public static bool IsReadableFile(string path)
    {
        if (!IsAbsolutePath(path) || !File.Exists(path))
            return false;
        try
        {
            using FileStream stream = File.Open(
                path,
                FileMode.Open,
                FileAccess.Read,
                FileShare.ReadWrite | FileShare.Delete);
            return stream.CanRead;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
        catch (IOException)
        {
            return false;
        }
    }

    public static bool IsOptionalReadableFile(string path) =>
        path.Length == 0 || IsReadableFile(path);
}
