using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Ludork.Plugin.Abstractions;
using Ludork.Services;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Views;

public partial class StartWindow : Window
{
    private static readonly FilePickerFileType ProjectFileType = new("Project Files")
    {
        Patterns = ["*.proj"],
    };

    private readonly EditorSettings editorSettings;

    public StartWindow() : this(EditorSettings.Load())
    {
    }

    public StartWindow(EditorSettings settings)
    {
        editorSettings = settings;
        editorSettings.removeMissingRecentProjects();
        InitializeComponent();
        NativeMenu? rootMenu = NativeMenu.GetMenu(this);
        if (Application.Current is App app
            && rootMenu?.Items.Count > 0
            && rootMenu.Items[0] is NativeMenuItem { Menu: NativeMenu pluginsMenu })
        {
            app.installPluginMenus(
                this,
                null,
                (PluginMenuLocation.Plugins, pluginsMenu));
        }
        AddHandler(DragDrop.DragOverEvent, onDragOver);
        AddHandler(DragDrop.DropEvent, onDrop);
    }

    private async void onNewProject(object? sender, RoutedEventArgs args)
    {
        string? projectFilePath = await NewProjectWindow.ShowAsync(this, editorSettings);
        if (projectFilePath is not null)
            openProject(projectFilePath);
    }

    private async void onOpenProject(object? sender, RoutedEventArgs args)
    {
        IStorageFolder? startLocation = await StorageProvider.TryGetFolderFromPathAsync(
            new Uri(Path.GetFullPath(editorSettings.getLastPathOrHome()))
        );
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = LocaleService.Get("SELECT_PROJ_FILE"),
            AllowMultiple = false,
            SuggestedStartLocation = startLocation,
            FileTypeFilter = [ProjectFileType],
        });
        if (files.Count > 0)
            openProject(files[0].Path.LocalPath);
    }

    private void onRecentProject(object? sender, RoutedEventArgs args)
    {
        if (sender is Button { DataContext: RecentProjectViewModel project })
            openProject(project.ProjectFilePath);
    }

    private void onTitleBarPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (args.GetCurrentPoint(this).Properties.PointerUpdateKind == PointerUpdateKind.LeftButtonPressed)
            BeginMoveDrag(args);
    }

    private async void onImportPlugin(object? sender, EventArgs args)
    {
        if (Application.Current is App app)
            await app.importPluginAsync(this);
    }

    private async void onManagePlugins(object? sender, EventArgs args)
    {
        if (Application.Current is App app)
            await app.showPluginManagerAsync(this);
    }

    private void onDragOver(object? sender, DragEventArgs args)
    {
        args.DragEffects = getDroppedProject(args) is null ? DragDropEffects.None : DragDropEffects.Copy;
    }

    private void onDrop(object? sender, DragEventArgs args)
    {
        string? projectPath = getDroppedProject(args);
        if (projectPath is not null)
            openProject(projectPath);
    }

    private static string? getDroppedProject(DragEventArgs args)
    {
        return args.DataTransfer.TryGetFiles()
            ?.Select(file => file.Path.LocalPath)
            .FirstOrDefault(path => path.EndsWith(".proj", StringComparison.OrdinalIgnoreCase) && File.Exists(path));
    }

    private void openProject(string projectFilePath)
    {
        if (Application.Current is App app)
            app.openProject(this, projectFilePath);
    }
}
