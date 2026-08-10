using System;
using System.IO;

namespace Ludork.Services;

public static class EditorPaths
{
    private const string AppName = "Ludork";

    public static string IniDirectory
    {
        get
        {
            if (OperatingSystem.IsWindows())
                return Environment.CurrentDirectory;

            string path = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                AppName
            );
            Directory.CreateDirectory(path);
            return path;
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

    public static string IniFilePath => Path.Combine(IniDirectory, $"{AppName}.ini");
}
