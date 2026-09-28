using Avalonia.Controls;
using Ludork.Services;
using System;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal static class EditorResourceCreation
{
    public static async Task<string?> SelectJsonPathAsync(
        Window owner,
        string root,
        string? destinationPath,
        string titleKey,
        bool selectWhenWhitespace = true)
    {
        Directory.CreateDirectory(root);
        string? path = destinationPath;
        if (path is null || selectWhenWhitespace && string.IsNullOrWhiteSpace(path))
        {
            path = await FileSelectorDialog.ShowAsync(owner, root,
                FileSelectorDialog.FilesFilter("*.json"), LocaleService.Get(titleKey), save: true);
        }
        return path is null ? null : NormalizePath(path);
    }

    public static string NormalizePath(string path)
    {
        string fullPath = Path.GetFullPath(path);
        return Path.HasExtension(fullPath) ? fullPath : Path.ChangeExtension(fullPath, "json");
    }

    public static bool TryGetJsonKey(
        string root,
        string path,
        out string key,
        StringComparison extensionComparison = StringComparison.OrdinalIgnoreCase)
    {
        string relative = Path.GetRelativePath(root, path);
        if (!EditorPathSandbox.IsSameOrChildPath(root, path)
            || !string.Equals(Path.GetExtension(path), ".json", extensionComparison))
        {
            key = string.Empty;
            return false;
        }
        key = Path.ChangeExtension(relative, null)!.Replace('\\', '/');
        return true;
    }
}
