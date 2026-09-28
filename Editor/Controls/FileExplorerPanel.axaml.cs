using Avalonia;
using Avalonia.Automation;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Selection;
using Avalonia.Input;
using Avalonia.Input.Platform;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;

namespace Ludork.Controls;

public partial class FileExplorerPanel : UserControl
{
    private const string DragPrefix = "ludork-file-explorer:";
    private EditorExternalOpenTarget externalOpenTarget;
    private Point? dragStart;
    private ListBox? dragSource;
    private PointerPressedEventArgs? dragPress;
    private bool startingDrag;
    private FileExplorerViewModel? previewViewModel;

    public FileExplorerPanel()
    {
        InitializeComponent();
        EditorInputs.ApplyEditable(SearchBox);
        SearchBox.PlaceholderText = LocaleService.Get("FILE_EXPLORER_SEARCH");
        string searchHint = LocaleService.Get("FILE_EXPLORER_SEARCH_HINT");
        ToolTip.SetTip(SearchBox, searchHint);
        AutomationProperties.SetName(SearchBox, searchHint);
        initializeZoom();
        ListEntries.AddHandler(KeyDownEvent, onTreeKeyDown, RoutingStrategies.Tunnel);
        DataContextChanged += (_, _) =>
        {
            restoreExternalOpenTarget();
            updateViewMode();
            if (IsLoaded)
                refreshEntries();
        };
        Loaded += (_, _) =>
        {
            restoreExternalOpenTarget();
            updateViewMode();
            refreshEntries();
        };
        EffectiveViewportChanged += (_, _) => updatePreviewActivity();
        ViewModeButton.PropertyChanged += (_, args) =>
        {
            if (args.Property == ToggleButton.IsCheckedProperty)
                updateViewMode();
        };
        IconEntries.AddHandler(PointerPressedEvent, onPointerPressed, RoutingStrategies.Tunnel);
        ListEntries.AddHandler(PointerPressedEvent, onPointerPressed, RoutingStrategies.Tunnel);
        IconEntries.AddHandler(PointerCaptureLostEvent, onPointerCaptureLost, RoutingStrategies.Bubble, true);
        ListEntries.AddHandler(PointerCaptureLostEvent, onPointerCaptureLost, RoutingStrategies.Bubble, true);
        IconEntries.AddHandler(InputElement.ContextRequestedEvent, onContextRequested, RoutingStrategies.Tunnel);
        ListEntries.AddHandler(InputElement.ContextRequestedEvent, onContextRequested, RoutingStrategies.Tunnel);
        configureDropTarget(IconEntries);
        configureDropTarget(ListEntries);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        updatePreviewActivity();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        resetPendingDrag();
        foreach (FileExplorerItemControl row in this.GetVisualDescendants().OfType<FileExplorerItemControl>())
            row.Deactivate();
        previewViewModel = null;
        base.OnDetachedFromVisualTree(args);
    }

    private void updatePreviewActivity()
    {
        previewViewModel = DataContext as FileExplorerViewModel;
        foreach (FileExplorerItemControl row in this.GetVisualDescendants().OfType<FileExplorerItemControl>())
            row.RefreshPreviewActivity();
    }

    private async void refreshEntries()
    {
        if (DataContext is FileExplorerViewModel viewModel)
            await viewModel.EnsureLoadedAsync();
    }

    public async Task<bool> LocatePathAsync(string path)
    {
        bool located = DataContext is FileExplorerViewModel viewModel && await viewModel.LocatePathAsync(path);
        if (located)
        {
            activeEntries.ScrollIntoView(activeEntries.SelectedItem!);
            activeEntries.Focus();
        }
        return located;
    }

    private async void onUp(object? sender, RoutedEventArgs args)
    {
        if (DataContext is FileExplorerViewModel viewModel)
            await viewModel.GoUpAsync();
    }

    private async void onBreadcrumbClick(object? sender, RoutedEventArgs args)
    {
        if (sender is Button { CommandParameter: string path } && DataContext is FileExplorerViewModel viewModel)
            await viewModel.NavigateToAsync(path);
    }

    private async void onRootSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (DataContext is FileExplorerViewModel viewModel
            && RootEntries.SelectedItem is FileExplorerRootViewModel root
            && root != viewModel.CurrentRoot)
        {
            await viewModel.NavigateToAsync(root.Path);
        }
    }

    private async void onRootTapped(object? sender, TappedEventArgs args)
    {
        if (args.Source is Control { DataContext: FileExplorerRootViewModel root }
            && DataContext is FileExplorerViewModel viewModel
            && !string.Equals(viewModel.CurrentPath, root.Path,
                OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
        {
            await viewModel.NavigateToAsync(root.Path);
        }
    }

    internal void UpdateItemPresentation(FileExplorerItemControl row, FileExplorerEntryViewModel item)
    {
        ListBox? list = row.FindAncestorOfType<ListBox>();
        Point? origin = list is null ? null : row.TranslatePoint(default, list);
        bool active = IsEffectivelyVisible && row.IsEffectivelyVisible && ReferenceEquals(list, activeEntries)
            && VisualRoot is not null && row.Bounds.Height > 0 && origin is not null;
        Rect bounds = new(origin ?? default, row.Bounds.Size);
        Rect viewport = new(list?.Bounds.Size ?? default);
        bool visible = active && bounds.Intersects(viewport);
        bool preload = active && bounds.Intersects(viewport.Inflate(new Thickness(0, viewport.Height)));
        double logicalSize = ReferenceEquals(list, IconEntries) ? previewViewModel?.IconSize ?? 64 : previewViewModel?.ListIconSize ?? 28;
        int size = (int)Math.Ceiling(logicalSize * (TopLevel.GetTopLevel(this)?.RenderScaling ?? 1));
        item.SetPresentation(row, visible, preload, size);
    }

    private void updateViewMode()
    {
        ListBox previous = activeEntries;
        FileExplorerViewModel? viewModel = DataContext as FileExplorerViewModel;
        bool iconMode = ViewModeButton.IsChecked == true;
        FileExplorerEntryViewModel[] selected = getSelectedEntries(previous)
            .Where(entry => !iconMode || entry.Depth == 0).ToArray();
        FileExplorerEntryViewModel? selectedItem = viewModel?.SelectedEntry
            ?? previous.SelectedItem as FileExplorerEntryViewModel;
        if (iconMode && viewModel is not null)
        {
            while (selectedItem is not null && viewModel.GetParentEntry(selectedItem) is FileExplorerEntryViewModel parent)
                selectedItem = parent;
        }
        int anchorIndex = previous.Selection.AnchorIndex;
        object? anchor = anchorIndex >= 0 && anchorIndex < previous.Items.Count ? previous.Items[anchorIndex] : null;
        if (iconMode && viewModel is not null)
        {
            while (anchor is FileExplorerEntryViewModel entry && viewModel.GetParentEntry(entry) is FileExplorerEntryViewModel parent)
                anchor = parent;
        }
        IconEntries.IsVisible = iconMode;
        ListEntries.IsVisible = !iconMode;
        ListBox current = activeEntries;
        using (current.Selection.BatchUpdate())
        {
            current.Selection.Clear();
            int selectedIndex = selectedItem is null ? -1 : current.Items.IndexOf(selectedItem);
            if (selectedIndex >= 0)
                current.Selection.Select(selectedIndex);
            foreach (FileExplorerEntryViewModel entry in selected)
            {
                int index = current.Items.IndexOf(entry);
                if (index >= 0 && index != selectedIndex)
                    current.Selection.Select(index);
            }
            current.Selection.AnchorIndex = anchor is null ? -1 : current.Items.IndexOf(anchor);
        }
        current.Focus();
        updatePreviewActivity();
    }

    private async void onDoubleTapped(object? sender, TappedEventArgs args)
    {
        if (getDirectoryExpander(args.Source) is not null)
        {
            args.Handled = true;
            return;
        }
        if (getEntry(args.Source) is not null && DataContext is FileExplorerViewModel viewModel)
            await viewModel.OpenSelectedAsync();
    }

    private async void onOpenTarget(object? sender, RoutedEventArgs args)
    {
        applyExternalOpenTarget(resolveExternalOpenTarget(externalOpenTarget));
        if (externalOpenTarget == EditorExternalOpenTarget.Folder)
        {
            await openContainingFolder();
            return;
        }
        if (externalOpenTarget == EditorExternalOpenTarget.VsCode)
        {
            await openExternalIde(ExternalIde.VsCode, "Visual Studio Code");
            return;
        }
        if (externalOpenTarget == EditorExternalOpenTarget.Cursor)
        {
            await openExternalIde(ExternalIde.Cursor, "Cursor");
            return;
        }
        if (externalOpenTarget == EditorExternalOpenTarget.Clion)
        {
            await openExternalIde(ExternalIde.Clion, "CLion");
            return;
        }
        await openExternalIde(ExternalIde.VisualStudio, "Visual Studio");
    }

    private void onSelectFolder(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(EditorExternalOpenTarget.Folder);
    }

    private void onSelectVsCode(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(EditorExternalOpenTarget.VsCode);
    }

    private void onSelectCursor(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(EditorExternalOpenTarget.Cursor);
    }

    private void onSelectClion(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(EditorExternalOpenTarget.Clion);
    }

    private void onSelectVisualStudio(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(EditorExternalOpenTarget.VisualStudio);
    }

    private void restoreExternalOpenTarget()
    {
        EditorExternalOpenTarget target = EditorLayoutService.Settings?.FileExplorerOpenTarget
            ?? EditorExternalOpenTarget.Folder;
        applyExternalOpenTarget(resolveExternalOpenTarget(target));
    }

    private EditorExternalOpenTarget resolveExternalOpenTarget(EditorExternalOpenTarget target)
    {
        if (DataContext is not FileExplorerViewModel viewModel)
            return EditorExternalOpenTarget.Folder;
        bool available = target switch
        {
            EditorExternalOpenTarget.Folder => true,
            EditorExternalOpenTarget.VsCode => viewModel.HasVSCode,
            EditorExternalOpenTarget.Cursor => viewModel.HasCursor,
            EditorExternalOpenTarget.Clion => viewModel.HasClion,
            EditorExternalOpenTarget.VisualStudio => viewModel.HasVisualStudio,
            _ => false,
        };
        return available ? target : EditorExternalOpenTarget.Folder;
    }

    private void selectExternalOpenTarget(EditorExternalOpenTarget target)
    {
        if (EditorLayoutService.Settings is EditorSettings settings)
        {
            settings.FileExplorerOpenTarget = target;
            EditorLayoutService.Save();
        }
        applyExternalOpenTarget(resolveExternalOpenTarget(target));
    }

    private void applyExternalOpenTarget(EditorExternalOpenTarget target)
    {
        externalOpenTarget = target;
        FolderOpenButtonIcon.IsVisible = target == EditorExternalOpenTarget.Folder;
        VSCodeButtonIcon.IsVisible = target == EditorExternalOpenTarget.VsCode;
        CursorButtonIcon.IsVisible = target == EditorExternalOpenTarget.Cursor;
        ClionButtonIcon.IsVisible = target == EditorExternalOpenTarget.Clion;
        VisualStudioButtonIcon.IsVisible = target == EditorExternalOpenTarget.VisualStudio;
    }

    private async Task openContainingFolder()
    {
        if ((DataContext as FileExplorerViewModel)?.OpenCurrentFolder() != false)
            return;
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        await AlertDialog.ShowAsync(
            owner,
            LocaleService.Get("ERROR"),
            LocaleService.Get("OPEN_CONTAINING_FOLDER_FAILED")
        );
    }

    private async Task openExternalIde(ExternalIde ide, string displayName)
    {
        if (DataContext is FileExplorerViewModel { IsReadOnly: true })
            return;
        if (DataContext is not FileExplorerViewModel viewModel)
            return;
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        if (viewModel.RequiresIdeInitialization(ide))
        {
            string prompt = LocaleService.Get("INITIALIZE_IDE_ENVIRONMENT_CONFIRM")
                .Replace("{app}", displayName);
            bool confirmed = await ConfirmationDialog.ShowAsync(
                owner,
                LocaleService.Get("INITIALIZE_IDE_ENVIRONMENT"),
                prompt);
            if (!confirmed)
                return;
            ExternalOpenButton.IsEnabled = false;
            IdeInitializationResult result;
            try
            {
                result = await viewModel.InitializeIdeAsync(ide);
            }
            finally
            {
                ExternalOpenButton.IsEnabled = true;
            }
            if (!result.Succeeded)
            {
                string failure = LocaleService.Get("INITIALIZE_IDE_ENVIRONMENT_FAILED")
                    .Replace("{app}", displayName)
                    .Replace("{details}", result.Details);
                await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), failure);
                return;
            }
        }
        if (viewModel.OpenExternalIde(ide))
            return;
        string message = LocaleService.Get("OPEN_EXTERNAL_EDITOR_FAILED").Replace("{app}", displayName);
        await AlertDialog.ShowAsync(
            owner,
            LocaleService.Get("ERROR"),
            message
        );
    }

    private static bool isScrollBarSource(object? source)
    {
        if (source is ScrollBar)
            return true;
        return (source as Visual)?.GetVisualAncestors().OfType<ScrollBar>().Any() == true;
    }

    private async void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        resetPendingDrag();
        if (getDirectoryExpander(args.Source) is ToggleButton { DataContext: FileExplorerEntryViewModel entry }
            && args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            args.Handled = true;
            if (DataContext is FileExplorerViewModel viewModel)
                await viewModel.ToggleDirectoryAsync(entry);
            return;
        }
        if (DataContext is not FileExplorerViewModel { IsReadOnly: false }
            || isScrollBarSource(args.Source)
            || getEntry(args.Source) is null)
            return;
        ListBox list = sender as ListBox ?? activeEntries;
        PointerPoint point = args.GetCurrentPoint(list);
        if (point.Properties.IsLeftButtonPressed
            && !point.Properties.IsRightButtonPressed
            && !point.Properties.IsMiddleButtonPressed)
        {
            dragStart = args.GetPosition(list);
            dragSource = list;
            dragPress = args;
            return;
        }
    }

    private void onContextRequested(
        object? sender,
        ContextRequestedEventArgs args)
    {
        resetPendingDrag();
        if (DataContext is not FileExplorerViewModel viewModel)
            return;
        ListBox list = sender as ListBox ?? activeEntries;
        bool requestedByPointer = args.TryGetPosition(list, out _);
        FileExplorerEntryViewModel? item = getEntry(args.Source);
        if (item is null && !requestedByPointer)
        {
            item = list.SelectedItem as FileExplorerEntryViewModel
                ?? viewModel.SelectedEntry;
        }
        if (item is not null && !getSelectedEntries(list).Contains(item))
        {
            using (list.Selection.BatchUpdate())
            {
                list.Selection.Clear();
                list.Selection.Select(viewModel.Entries.IndexOf(item));
            }
        }
        Control placementTarget = list;
        if (!requestedByPointer)
        {
            placementTarget = item is not null
                ? list.ContainerFromItem(item) ?? args.Source as Control ?? list
                : args.Source as Control ?? list;
        }
        openContextMenu(
            viewModel,
            list,
            item,
            requestedByPointer,
            placementTarget);
        args.Handled = true;
    }

    private void openContextMenu(
        FileExplorerViewModel viewModel,
        ListBox list,
        FileExplorerEntryViewModel? item,
        bool requestedByPointer,
        Control placementTarget)
    {
        if (viewModel.IsReadOnly)
            return;
        IReadOnlyList<FileExplorerEntryViewModel> selected = getSelectedEntries(list);
        IReadOnlyList<FileExplorerEntryViewModel> selectedFiles = selected
            .Where(entry => !entry.IsDirectory)
            .ToArray();
        ContextMenu menu = new ContextMenu();
        string targetDirectory = item?.IsDirectory == true ? item.FullPath : viewModel.CurrentPath;
        List<object> items = [];
        addNewDataItems(items, viewModel, targetDirectory);
        MenuItem newFolder = new MenuItem { Header = LocaleService.Get("NEW_FOLDER") };
        newFolder.Click += async (_, _) => await createFolder(viewModel, targetDirectory);
        items.Add(newFolder);
        if (item is not null && selected.Count != 0)
        {
            items.Add(new Separator());
            MenuItem copy = new MenuItem { Header = LocaleService.Get("COPY") };
            copy.Click += (_, _) => viewModel.SetClipboard(selected.Select(entry => entry.FullPath), false);
            items.Add(copy);
            MenuItem cut = new MenuItem { Header = LocaleService.Get("CUT") };
            cut.Click += (_, _) => viewModel.SetClipboard(selected.Select(entry => entry.FullPath), true);
            items.Add(cut);
        }
        if (viewModel.HasClipboard)
        {
            MenuItem paste = new MenuItem { Header = LocaleService.Get("PASTE") };
            paste.Click += async (_, _) => await showOperationErrors(viewModel.Paste(targetDirectory), LocaleService.Get("ERROR"));
            items.Add(paste);
        }
        if (item is not null)
        {
            items.Add(new Separator());
            MenuItem openSystem = new MenuItem { Header = LocaleService.Get("OPEN_FROM_SYSTEM") };
            openSystem.Click += async (_, _) => await openFromSystem(item.FullPath);
            items.Add(openSystem);
            if (!item.IsDirectory && tryGetBlueprintReference(viewModel, item.FullPath, out string blueprintReference))
            {
                MenuItem copyClass = new MenuItem { Header = LocaleService.Get("COPY_BLUEPRINT_CLASS_NAME") };
                copyClass.Click += async (_, _) => await copyBlueprintClassName(blueprintReference);
                items.Add(copyClass);
                MenuItem derive = new MenuItem { Header = LocaleService.Get("DERIVE_FROM_THIS_BLUEPRINT") };
                derive.Click += (_, _) => viewModel.RequestDataCreation(new EditorDataCreationRequest(EditorDataKind.Blueprint, ParentClass: blueprintReference));
                items.Add(derive);
            }
            if (!item.IsDirectory && viewModel.CanShowReferenceTree(item.FullPath))
            {
                MenuItem references = new MenuItem { Header = LocaleService.Get("SHOW_REFERENCE_TREE") };
                references.Click += (_, _) => viewModel.RequestReferenceTree(item.FullPath);
                items.Add(references);
            }
            if (selectedFiles.Count != 0)
            {
                MenuItem duplicate = new MenuItem { Header = LocaleService.Get("DUPLICATE_FILE") };
                duplicate.Click += async (_, _) => await showOperationErrors(
                    viewModel.Duplicate(selectedFiles.Select(entry => entry.FullPath)),
                    LocaleService.Get("DUPLICATE_FAILED"));
                items.Add(duplicate);
            }
            if (selected.Count == 1)
            {
                MenuItem rename = new MenuItem { Header = LocaleService.Get("RENAME_FILE") };
                rename.Click += async (_, _) => await renameSelected(viewModel, item);
                items.Add(rename);
            }
            MenuItem delete = new MenuItem { Header = LocaleService.Get("DELETE") };
            delete.Click += async (_, _) => await deleteSelected(viewModel, selected);
            items.Add(delete);
        }
        menu.ItemsSource = items;
        menu.Placement = requestedByPointer
            ? PlacementMode.Pointer
            : PlacementMode.Bottom;
        menu.PlacementTarget = placementTarget;
        menu.Open(list);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (DataContext is not FileExplorerViewModel viewModel)
            return;
        if (SearchBox.IsKeyboardFocusWithin)
        {
            if (args.Key == Key.F5)
            {
                args.Handled = true;
                await viewModel.RefreshAsync();
            }
            return;
        }
        KeyModifiers modifiers = args.KeyModifiers;
        if (RootEntries.IsKeyboardFocusWithin)
        {
            if (args.Key is Key.Enter or Key.Space)
            {
                args.Handled = true;
                if (RootEntries.SelectedItem is FileExplorerRootViewModel root)
                    await viewModel.NavigateToAsync(root.Path);
            }
            else if (args.Key is Key.Delete or Key.F2 or Key.Back
                || EditorShortcuts.HasPrimaryModifier(modifiers) && args.Key is Key.C or Key.X or Key.V or Key.D or Key.N or Key.A)
            {
                args.Handled = true;
            }
            return;
        }
        if (viewModel.IsReadOnly)
        {
            if (args.Key == Key.Enter)
            {
                await viewModel.OpenSelectedAsync();
                args.Handled = true;
                return;
            }
            if (args.Key is Key.Delete or Key.F2
                || EditorShortcuts.HasPrimaryModifier(modifiers) && args.Key is Key.X or Key.V or Key.D or Key.N)
            {
                args.Handled = true;
                return;
            }
        }
        if (EditorShortcuts.HasPrimaryModifier(modifiers))
        {
            IReadOnlyList<FileExplorerEntryViewModel> selected = getSelectedEntries(activeEntries);
            if (args.Key == Key.C) viewModel.SetClipboard(selected.Select(entry => entry.FullPath), false);
            else if (args.Key == Key.X) viewModel.SetClipboard(selected.Select(entry => entry.FullPath), true);
            else if (args.Key == Key.V) await showOperationErrors(viewModel.Paste(), LocaleService.Get("ERROR"));
            else if (args.Key == Key.D) await showOperationErrors(viewModel.Duplicate(selected.Select(entry => entry.FullPath)), LocaleService.Get("DUPLICATE_FAILED"));
            else if (args.Key == Key.N && modifiers.HasFlag(KeyModifiers.Shift)) await createFolder(viewModel, viewModel.CurrentPath);
            else if (args.Key == Key.A) activeEntries.SelectAll();
            else return;
            args.Handled = true;
            return;
        }
        switch (args.Key)
        {
            case Key.Back:
                await viewModel.GoUpAsync();
                break;
            case Key.F5:
                await viewModel.RefreshAsync();
                break;
            case Key.Delete:
                await deleteSelected(viewModel, getSelectedEntries(activeEntries));
                break;
            case Key.F2:
                if (OperatingSystem.IsMacOS())
                    return;
                await tryRenameSelected(viewModel);
                break;
            case Key.Enter:
                if (OperatingSystem.IsMacOS())
                    await tryRenameSelected(viewModel);
                else
                    await viewModel.OpenSelectedAsync();
                break;
            case Key.Space:
                if (viewModel.SelectedEntry is { IsDirectory: false } selected && isImage(selected.FullPath)
                    && TopLevel.GetTopLevel(this) is Window owner)
                    await new FilePreviewDialog(selected.FullPath, viewModel.Thumbnails).ShowDialog(owner);
                break;
            default:
                return;
        }
        args.Handled = true;
    }

    private async Task createFolder(FileExplorerViewModel viewModel, string targetDirectory)
    {
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        IEnumerable<string> existing = Directory.Exists(targetDirectory)
            ? Directory.EnumerateFileSystemEntries(targetDirectory).Select(Path.GetFileName).OfType<string>()
            : [];
        string? name = await SingleRowDialog.ShowAsync(owner, LocaleService.Get("NEW_FOLDER"), LocaleService.Get("NEW_FOLDER_PROMPT"), existing);
        if (!string.IsNullOrWhiteSpace(name))
        {
            await showOperationErrors(
                viewModel.CreateDirectory(name, targetDirectory),
                LocaleService.Get("CREATE_FOLDER_FAILED"));
        }
    }

    private async Task tryRenameSelected(FileExplorerViewModel viewModel)
    {
        IReadOnlyList<FileExplorerEntryViewModel> selected = getSelectedEntries(activeEntries);
        if (selected.Count != 1)
            return;
        await renameSelected(viewModel, selected[0]);
    }

    private async Task renameSelected(FileExplorerViewModel viewModel, FileExplorerEntryViewModel item)
    {
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        string? name = await SingleRowDialog.ShowAsync(owner, LocaleService.Get("RENAME_FILE"), LocaleService.Get("RENAME_FILE"), viewModel.GetSiblingNames(item), item.Name);
        if (!string.IsNullOrWhiteSpace(name))
            await showOperationErrors(viewModel.RenameSelected(name), LocaleService.Get("RENAME_FAILED"));
    }

    private static bool isImage(string path) => new[] { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp" }
        .Contains(Path.GetExtension(path), StringComparer.OrdinalIgnoreCase);

    private static bool tryGetBlueprintReference(FileExplorerViewModel viewModel, string path, out string reference)
    {
        string root = Path.Combine(viewModel.ProjectPath, "Data", "Blueprints");
        string relative = Path.GetRelativePath(root, path);
        if (Path.IsPathRooted(relative)
            || relative == ".."
            || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            || !string.Equals(Path.GetExtension(path), ".json", StringComparison.OrdinalIgnoreCase))
        {
            reference = string.Empty;
            return false;
        }
        string key = BlueprintReference.NormalizeKey(relative);
        if (!viewModel.HasBlueprint(key))
        {
            reference = string.Empty;
            return false;
        }
        reference = BlueprintReference.ToReference(key);
        return true;
    }

    private async Task copyBlueprintClassName(string reference)
    {
        IClipboard? clipboard = TopLevel.GetTopLevel(this)?.Clipboard;
        if (clipboard is not null)
            await clipboard.SetTextAsync(reference);
    }

    private void addNewDataItems(List<object> items, FileExplorerViewModel viewModel, string targetDirectory)
    {
        (EditorDataKind Kind, string LocaleKey, string Root, string? DataType)[] roots =
        [
            (EditorDataKind.Blueprint, "NEW_BLUEPRINT", Path.Combine(viewModel.ProjectPath, "Data", "Blueprints"), null),
            (EditorDataKind.Animation, "NEW_ANIMATION", Path.Combine(viewModel.ProjectPath, "Data", "Animations"), null),
            (EditorDataKind.Particle, "NEW_PARTICLE", Path.Combine(viewModel.ProjectPath, "Data", "Particles"), null),
            (EditorDataKind.Subtitle, "NEW_SUBTITLE", Path.Combine(viewModel.ProjectPath, "Assets", "Subtitles"), null),
            (EditorDataKind.Curve, "NEW_CURVE", Path.Combine(viewModel.ProjectPath, "Data", "Curves"), "curve"),
            (EditorDataKind.Curve, "NEW_VECTOR2_CURVE", Path.Combine(viewModel.ProjectPath, "Data", "Curves"), "vector2Curve"),
            (EditorDataKind.Curve, "NEW_VECTOR3_CURVE", Path.Combine(viewModel.ProjectPath, "Data", "Curves"), "vector3Curve"),
            (EditorDataKind.Curve, "NEW_VECTOR4_CURVE", Path.Combine(viewModel.ProjectPath, "Data", "Curves"), "vector4Curve"),
            (EditorDataKind.TextConfig, "NEW_TEXT_CONFIG", Path.Combine(viewModel.ProjectPath, "Data", "TextConfigs"), null),
            (EditorDataKind.UiAsset, "NEW_UI_ASSET", Path.Combine(viewModel.ProjectPath, "Data", "UI", "Assets"), null),
        ];
        foreach ((EditorDataKind kind, string localeKey, string root, string? dataType) in roots)
        {
            if (!isInside(targetDirectory, root))
                continue;
            MenuItem create = new MenuItem { Header = LocaleService.Get(localeKey) };
            create.Click += async (_, _) => await createDataFile(
                viewModel,
                kind,
                localeKey,
                targetDirectory,
                dataType);
            items.Add(create);
        }
    }

    private async Task createDataFile(
        FileExplorerViewModel viewModel,
        EditorDataKind kind,
        string localeKey,
        string targetDirectory,
        string? dataType)
    {
        if (kind == EditorDataKind.TextConfig)
        {
            viewModel.RequestDataCreation(new EditorDataCreationRequest(
                kind,
                InitialDirectory: targetDirectory));
            return;
        }
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        IEnumerable<string> existing = Directory.Exists(targetDirectory)
            ? Directory.EnumerateFileSystemEntries(targetDirectory).Select(Path.GetFileName).OfType<string>()
            : [];
        string? name = await SingleRowDialog.ShowAsync(owner, LocaleService.Get(localeKey), LocaleService.Get("FILE_NAME"), existing);
        string trimmed = name?.Trim() ?? string.Empty;
        if (trimmed.Length == 0
            || trimmed is "." or ".."
            || trimmed.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
            || trimmed.Contains('/')
            || trimmed.Contains('\\'))
        {
            return;
        }
        string extension = Path.GetExtension(trimmed);
        if (extension.Length == 0)
            trimmed += ".json";
        else if (!string.Equals(extension, ".json", StringComparison.OrdinalIgnoreCase))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), LocaleService.Get("INVALID_FILE_NAME"));
            return;
        }
        viewModel.RequestDataCreation(new EditorDataCreationRequest(
            kind,
            Path.Combine(targetDirectory, trimmed),
            DataType: dataType));
    }

    private async Task deleteSelected(
        FileExplorerViewModel viewModel,
        IReadOnlyList<FileExplorerEntryViewModel> selected)
    {
        if (selected.Count == 0 || TopLevel.GetTopLevel(this) is not Window owner)
            return;
        bool confirmed = await ConfirmationDialog.ShowAsync(
            owner,
            LocaleService.Get("CONFIRM_DELETE"),
            LocaleService.Get("DELETE_DOCUMENT_CONFIRMATION"));
        if (!confirmed)
            return;
        await showOperationErrors(
            viewModel.Delete(selected.Select(entry => entry.FullPath)),
            LocaleService.Get("DELETE_FAILED"));
    }

    private async Task openFromSystem(string path)
    {
        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(path) { UseShellExecute = true });
        }
        catch (Exception exception)
        {
            if (TopLevel.GetTopLevel(this) is Window owner)
                await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), exception.Message);
        }
    }

    private async Task showOperationErrors(FileOperationResult result, string title)
    {
        if (result.Errors.Count == 0 || TopLevel.GetTopLevel(this) is not Window owner)
            return;
        await AlertDialog.ShowAsync(owner, title, string.Join(Environment.NewLine, result.Errors));
    }

    private void configureDropTarget(ListBox list)
    {
        DragDrop.SetAllowDrop(list, true);
        list.AddHandler(DragDrop.DragOverEvent, onDragOver);
        list.AddHandler(DragDrop.DropEvent, onDrop);
    }

    private async void onPointerMoved(object? sender, PointerEventArgs args)
    {
        if (DataContext is FileExplorerViewModel { IsReadOnly: true })
            return;
        if (isScrollBarSource(args.Source))
        {
            resetPendingDrag();
            return;
        }
        if (startingDrag
            || dragStart is not Point start
            || dragSource is not ListBox list
            || dragPress is not PointerPressedEventArgs press)
            return;
        PointerPoint point = args.GetCurrentPoint(list);
        if (!point.Properties.IsLeftButtonPressed
            || point.Properties.IsRightButtonPressed
            || point.Properties.IsMiddleButtonPressed)
        {
            resetPendingDrag();
            return;
        }
        Point current = args.GetPosition(list);
        if (Math.Abs(current.X - start.X) < 4 && Math.Abs(current.Y - start.Y) < 4)
            return;
        string[] paths = getSelectedEntries(list).Select(entry => entry.FullPath).ToArray();
        if (paths.Length == 0)
            return;
        startingDrag = true;
        DataTransfer data = new();
        data.Add(DataTransferItem.CreateText(DragPrefix + JsonSerializer.Serialize(paths)));
        await DragDrop.DoDragDropAsync(press, data, DragDropEffects.Move);
        startingDrag = false;
        resetPendingDrag();
    }

    private void onPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        resetPendingDrag();
    }

    private void onPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        resetPendingDrag();
    }

    private void resetPendingDrag()
    {
        dragStart = null;
        dragSource = null;
        dragPress = null;
    }

    private void onDragOver(object? sender, DragEventArgs args)
    {
        if (DataContext is not FileExplorerViewModel viewModel
            || viewModel.IsReadOnly
            || getDraggedPaths(args) is not { Count: > 0 } paths
            || paths.Any(path => !viewModel.IsUnderRoot(path)))
        {
            args.DragEffects = DragDropEffects.None;
            return;
        }
        args.DragEffects = DragDropEffects.Move;
        args.Handled = true;
    }

    private async void onDrop(object? sender, DragEventArgs args)
    {
        if (DataContext is not FileExplorerViewModel viewModel
            || viewModel.IsReadOnly
            || sender is not ListBox list
            || getDraggedPaths(args) is not { Count: > 0 } paths)
        {
            return;
        }
        string targetDirectory = getDropTargetDirectory(list, args, viewModel);
        await showOperationErrors(viewModel.Move(paths, targetDirectory), LocaleService.Get("MOVE_FILE_FAILED"));
        args.Handled = true;
    }

    private static IReadOnlyList<string>? getDraggedPaths(DragEventArgs args)
    {
        string? text = args.DataTransfer.TryGetText();
        if (text is null || !text.StartsWith(DragPrefix, StringComparison.Ordinal))
            return null;
        try
        {
            return JsonSerializer.Deserialize<string[]>(text[DragPrefix.Length..]);
        }
        catch (JsonException)
        {
            return null;
        }
    }

    private static string getDropTargetDirectory(
        ListBox list,
        DragEventArgs args,
        FileExplorerViewModel viewModel)
    {
        Visual? hit = list.InputHitTest(args.GetPosition(list)) as Visual;
        FileExplorerEntryViewModel? entry = hit?.GetVisualAncestors()
            .OfType<ListBoxItem>()
            .Select(item => item.DataContext as FileExplorerEntryViewModel)
            .FirstOrDefault(item => item is not null);
        return entry?.IsDirectory == true ? entry.FullPath : viewModel.CurrentPath;
    }

    private static IReadOnlyList<FileExplorerEntryViewModel> getSelectedEntries(ListBox list)
    {
        return list.SelectedItems?.OfType<FileExplorerEntryViewModel>().ToArray() ?? [];
    }

    private static FileExplorerEntryViewModel? getEntry(object? source)
    {
        if (source is ListBoxItem item)
            return item.DataContext as FileExplorerEntryViewModel;
        return (source as Visual)?.GetVisualAncestors()
            .OfType<ListBoxItem>()
            .Select(container => container.DataContext as FileExplorerEntryViewModel)
            .FirstOrDefault(entry => entry is not null);
    }

    private static bool isInside(string path, string root)
    {
        string relative = Path.GetRelativePath(Path.GetFullPath(root), Path.GetFullPath(path));
        return relative == "."
            || (!Path.IsPathRooted(relative)
                && relative != ".."
                && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal));
    }

    private ListBox activeEntries => IconEntries.IsVisible ? IconEntries : ListEntries;
}
