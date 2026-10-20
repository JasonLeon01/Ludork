using System;
using System.IO;

namespace Ludork.Services;

public static class EditorPaths
{
    private const string AppName = "Ludork";

    public static string UserDataDirectory
    {
        get
        {
            string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            if (string.IsNullOrWhiteSpace(userProfile) || !Path.IsPathFullyQualified(userProfile))
                throw new DirectoryNotFoundException("The current user's profile directory is unavailable.");
            return Path.Combine(userProfile, AppName);
        }
    }

    public static string ProjectsDirectory
    {
        get
        {
            string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            if (string.IsNullOrWhiteSpace(userProfile))
                userProfile = Environment.CurrentDirectory;
            string path = Path.Combine(userProfile, "LudorkProjects");
            Directory.CreateDirectory(path);
            return Path.GetFullPath(path);
        }
    }

    public static string ConfigFilePath => Path.Combine(UserDataDirectory, ConfigFileNames.GetFileName(AppName));

    public static string TemplateCacheDirectory => Path.Combine(
        UserDataDirectory, "Templates", "v" + EditorVersionService.BaseVersion);

    public static string WebViewDataDirectory => Path.Combine(UserDataDirectory, "WebView2");

    public static string GetProjectLogFilePath(string projectPath) => Path.Combine(
        UserDataDirectory, "Log", EditorProjectIdentity.GetKey(projectPath), "Ludork.log");
}
