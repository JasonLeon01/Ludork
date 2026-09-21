using Ludork.Plugin.Abstractions;
using Ludork.Services.ResourceCleanup;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;

namespace Ludork.Views;

public partial class MainWindow
{
    internal IResourceCleanupHost? CreateResourceCleanupHost()
    {
        if (viewModel is not MainViewModel current)
            return null;
        return new ResourceCleanupHostBridge(current.GameData, () =>
        {
            if (viewModel != current || !current.CanEdit)
                throw new InvalidOperationException("Stop the running project before cleaning resources.");
            current.ProjectSave.FlushPendingChanges();
        }, paths =>
        {
            current.GameData.AcceptTrashedResources(paths);
            current.ReferenceIndex.MarkDirty();
            current.FileExplorerPanel.RequestRefresh();
            documentWindows?.ApplyFileChanges(this, new FileExplorerFilesChangedEventArgs([], [], paths));
        });
    }
}
