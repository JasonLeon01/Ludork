using Avalonia.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class MainWindow
{
    private async Task createParticleAsync(GameDataService gameData, string? destinationPath = null)
    {
        string root = Path.Combine(gameData.ProjectPath, "Data", "Particles");
        Directory.CreateDirectory(root);
        string? path = destinationPath ?? await FileSelectorDialog.ShowAsync(this, root,
            FileSelectorDialog.FilesFilter("*.json"), LocaleService.Get("SELECT_PARTICLE_PATH"), save: true);
        if (path is null)
            return;
        path = Path.GetFullPath(path);
        if (!Path.HasExtension(path))
            path = Path.ChangeExtension(path, "json");
        string relative = Path.GetRelativePath(root, path);
        if (!isRelativePathInside(relative) || !string.Equals(Path.GetExtension(path), ".json", StringComparison.Ordinal))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_PARTICLE_PATH"));
            return;
        }
        string key = Path.ChangeExtension(relative, null)!.Replace('\\', '/');
        if (!gameData.CreateParticle(key, Path.GetFileNameWithoutExtension(key)))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("PARTICLE_EXISTS"));
            return;
        }
        showParticle(key, gameData);
    }

    private void showParticleOverview(GameDataService gameData)
    {
        if (particleOverview is not null)
        {
            particleOverview.Activate();
            return;
        }
        particleOverview = new ParticleOverviewWindow(gameData, viewModel!.ProjectSave,
            viewModel.UiControlRegistry.Runtime, () => createParticleAsync(gameData));
        particleOverview.Closed += (_, _) => particleOverview = null;
        particleOverview.Show(this);
    }

    private void showParticle(string key, GameDataService gameData)
    {
        ParticleWindow? existing = OwnedWindows.OfType<ParticleWindow>().FirstOrDefault(window => window.Key == key);
        if (existing is not null)
        {
            existing.Activate();
            return;
        }
        if (gameData.ParticlesData.ContainsKey(key))
            new ParticleWindow(gameData, viewModel!.ProjectSave, viewModel.UiControlRegistry.Runtime, key).Show(this);
    }
}
