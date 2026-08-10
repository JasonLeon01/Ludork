using System.IO;

namespace Ludork.Plugins.OfficialLocaleTools;

internal static class LocaleProjectPaths
{
    public static string GetWorkbookPath(string projectPath)
    {
        return Path.Combine(projectPath, "Data", "Locale", "Locale.xlsx");
    }

    public static string GetOutputDirectory(string projectPath)
    {
        return Path.Combine(projectPath, "Scripts", "Source", "Locale");
    }
}
