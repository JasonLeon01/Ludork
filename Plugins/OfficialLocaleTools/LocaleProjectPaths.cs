using System.IO;

namespace Ludork.Plugins.OfficialLocaleTools;

internal static class LocaleProjectPaths
{
    public const string WorkbookRelativePath = "Data/Locale/Locale.xlsx";
    public const string OutputRelativeDirectory = "Scripts/Source/Locale";

    public static string GetWorkbookPath(string projectPath)
    {
        return Path.Combine(projectPath, WorkbookRelativePath.Replace('/', Path.DirectorySeparatorChar));
    }

    public static string GetOutputDirectory(string projectPath)
    {
        return Path.Combine(projectPath, OutputRelativeDirectory.Replace('/', Path.DirectorySeparatorChar));
    }
}
