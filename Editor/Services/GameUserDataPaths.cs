using System;
using System.IO;

namespace Ludork.Services;

internal static class GameUserDataPaths
{
    public static string GetMainConfigPath(string appName) =>
        Path.Combine(GetUserDataPath(appName), ConfigFileNames.GetFileName("Main"));

    public static string GetSavePath(string appName) => Path.Combine(GetUserDataPath(appName), "Save");

    private static string GetUserDataPath(string appName)
    {
        string? userProfile = OperatingSystem.IsWindows()
            ? Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)
            : Environment.GetEnvironmentVariable("HOME");
        if (string.IsNullOrEmpty(userProfile) || !Path.IsPathFullyQualified(userProfile))
            throw new DirectoryNotFoundException("The current user's home directory is unavailable.");
        return Path.Combine(Path.GetFullPath(userProfile), appName);
    }
}
