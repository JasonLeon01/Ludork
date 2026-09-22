using System;
using System.IO;
using System.Linq;

namespace Ludork.Services;

internal static class AppleSigningInput
{
    public static bool HasLineBreak(string value) =>
        value.IndexOfAny(['\r', '\n']) >= 0;

    public static bool IsNonEmptySingleLine(string value) =>
        value.Length != 0 && !HasLineBreak(value);

    public static bool IsValidTeamId(string value) =>
        value.Length == 10
        && value.All(character =>
            (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9'));

    public static bool IsReadableFile(string path)
    {
        if (!Path.IsPathFullyQualified(path)
            || HasLineBreak(path)
            || !File.Exists(path))
        {
            return false;
        }
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
