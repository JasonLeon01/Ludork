using Avalonia.Controls;
using Ludork.Services;
using System;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal static class MaterialAuthoring
{
    public static async Task<string?> CreateAsync(Window owner, ProjectDataStore gameData, PreviewRuntimeService runtime, string? destination = null)
    {
        string root = Path.Combine(gameData.ProjectPath, "Data", "Materials");
        string? path = await EditorResourceCreation.SelectJsonPathAsync(owner, root, destination, "SELECT_MATERIAL");
        if (path is null || !EditorResourceCreation.TryGetJsonKey(root, path, out string key))
            return null;
        try
        {
            await using MaterialPreviewClient client = new(runtime);
            JsonObject registry = await client.DescribeAsync(CancellationToken.None);
            if (gameData.Assets.CreateMaterial(key, (JsonObject)registry["defaultMaterial"]!))
                return key;
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("MATERIAL_EXISTS"));
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), exception.Message);
        }
        return null;
    }
}
