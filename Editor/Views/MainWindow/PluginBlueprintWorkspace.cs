using Ludork.Plugin.Abstractions;
using Ludork.Services.Plugins;
using Ludork.ViewModels;

namespace Ludork.Views;

public partial class MainWindow
{
    internal IPluginBlueprintWorkspace? CreatePluginBlueprintWorkspace()
    {
        if (viewModel is not MainViewModel current)
            return null;
        return new PluginBlueprintWorkspace(current.GameData, () =>
        {
            if (viewModel != current || !current.CanEdit)
                throw new PluginWorkspaceUnavailableException();
            current.ProjectSave.FlushPendingChanges();
        }, paths =>
        {
            current.ReferenceIndex.MarkDirty();
            current.FileExplorers.ApplyFileChanges(new FileExplorerFilesChangedEventArgs(paths, [], []));
        });
    }
}
