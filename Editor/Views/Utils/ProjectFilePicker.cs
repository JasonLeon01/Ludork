using Avalonia.Controls;
using Avalonia.Platform.Storage;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal static class ProjectFilePicker
{
    private static readonly FilePickerFileType ProjectFileType = new("Project Files")
    {
        Patterns = ["*.proj"],
    };

    public static async Task<string?> PickAsync(Window owner, EditorSettings settings)
    {
        IStorageProvider storage = owner.StorageProvider;
        using IStorageFolder? location = await storage.TryGetFolderFromPathAsync(new Uri(settings.getLastPathOrHome()));
        IReadOnlyList<IStorageFile> files = await storage.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = LocaleService.Get("SELECT_PROJ_FILE"),
            AllowMultiple = false,
            SuggestedStartLocation = location,
            FileTypeFilter = [ProjectFileType],
        });
        using IStorageFile? file = files.Count == 0 ? null : files[0];
        return file?.TryGetLocalPath();
    }
}
