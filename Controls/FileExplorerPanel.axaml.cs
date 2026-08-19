using Avalonia;
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
    private ExternalOpenTarget externalOpenTarget;
    private Point? dragStart;
    private ListBox? dragSource;
    private PointerPressedEventArgs? dragPress;
    private bool startingDrag;
    private FileExplorerViewModel? previewViewModel;

    public FileExplorerPanel()
    {
        InitializeComponent();
        DataContextChanged += (_, _) =>
        {
            updatePreviewActivity();
            if (IsLoaded)
                refreshEntries();
        };
        Loaded += (_, _) =>
        {
            updatePreviewActivity();
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
        if (previewViewModel is not null)
            previewViewModel.SetPreviewActive(false);
        previewViewModel = null;
        base.OnDetachedFromVisualTree(args);
    }

    private void updatePreviewActivity()
    {
        FileExplorerViewModel? next = DataContext as FileExplorerViewModel;
        if (previewViewModel != next)
            previewViewModel?.SetPreviewActive(false);
        previewViewModel = next;
        previewViewModel?.SetPreviewActive(IsEffectivelyVisible && VisualRoot is not null);
    }

    private void refreshEntries()
    {
        if (DataContext is not FileExplorerViewModel viewModel)
            return;
        viewModel.Refresh();
    }

    public bool LocatePath(string path)
    {
        bool located = (DataContext as FileExplorerViewModel)?.LocatePath(path) == true;
        if (located)
            activeEntries.Focus();
        return located;
    }

    private void onUp(object? sender, RoutedEventArgs args) => (DataContext as FileExplorerViewModel)?.GoUp();
    private void onBreadcrumbClick(object? sender, RoutedEventArgs args)
    {
        if (sender is Button { CommandParameter: string path })
            (DataContext as FileExplorerViewModel)?.NavigateTo(path);
    }

    private void updateViewMode()
    {
        ListBox previous = activeEntries;
        int[] selectedIndexes = previous.Selection.SelectedIndexes.ToArray();
        int selectedIndex = previous.Selection.SelectedIndex;
        int anchorIndex = previous.Selection.AnchorIndex;
        bool iconMode = ViewModeButton.IsChecked == true;
        IconEntries.IsVisible = iconMode;
        ListEntries.IsVisible = !iconMode;
        ListBox current = activeEntries;
        using (current.Selection.BatchUpdate())
        {
            current.Selection.Clear();
            if (selectedIndex >= 0)
                current.Selection.Select(selectedIndex);
            foreach (int index in selectedIndexes)
            {
                if (index != selectedIndex)
                    current.Selection.Select(index);
            }
            current.Selection.AnchorIndex = anchorIndex;
        }
        current.Focus();
    }

    private void onDoubleTapped(object? sender, TappedEventArgs args) => (DataContext as FileExplorerViewModel)?.OpenSelected();

    private async void onOpenTarget(object? sender, RoutedEventArgs args)
    {
        if (externalOpenTarget == ExternalOpenTarget.Folder)
        {
            await openContainingFolder();
            return;
        }
        if (externalOpenTarget == ExternalOpenTarget.VsCode)
        {
            await openExternalEditor("code", "Visual Studio Code");
            return;
        }
        await openExternalEditor("cursor", "Cursor");
    }

    private void onSelectFolder(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(ExternalOpenTarget.Folder);
    }

    private void onSelectVsCode(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(ExternalOpenTarget.VsCode);
    }

    private void onSelectCursor(object? sender, RoutedEventArgs args)
    {
        selectExternalOpenTarget(ExternalOpenTarget.Cursor);
    }

    private void selectExternalOpenTarget(ExternalOpenTarget target)
    {
        externalOpenTarget = target;
        FolderOpenButtonIcon.IsVisible = target == ExternalOpenTarget.Folder;
        VSCodeButtonIcon.IsVisible = target == ExternalOpenTarget.VsCode;
        CursorButtonIcon.IsVisible = target == ExternalOpenTarget.Cursor;
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

    private async Task openExternalEditor(string command, string displayName)
    {
        if ((DataContext as FileExplorerViewModel)?.OpenExternalEditor(command) != false)
            return;
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        string message = LocaleService.Get("EXTERNAL_EDITOR_NOT_FOUND").Replace("{app}", displayName);
        await AlertDialog.ShowAsync(
            owner,
            LocaleService.Get("ERROR"),
            message
        );
    }

    private void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (DataContext is not FileExplorerViewModel viewModel)
            return;
        ListBox list = sender as ListBox ?? activeEntries;
        PointerPoint point = args.GetCurrentPoint(list);
        if (point.Properties.IsLeftButtonPressed)
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
        KeyModifiers modifiers = args.KeyModifiers;
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
                viewModel.GoUp();
                break;
            case Key.F5:
                viewModel.Refresh();
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
                    viewModel.OpenSelected();
                break;
            case Key.Space:
                if (viewModel.SelectedEntry is { IsDirectory: false } selected && isImage(selected.FullPath)
                    && TopLevel.GetTopLevel(this) is Window owner)
                    await new FilePreviewDialog(selected.FullPath).ShowDialog(owner);
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
        string? name = await SingleRowDialog.ShowAsync(owner, LocaleService.Get("RENAME_FILE"), LocaleService.Get("RENAME_FILE"), viewModel.Entries.Where(entry => entry != item).Select(entry => entry.Name), item.Name);
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
        string key = Path.ChangeExtension(relative, null)!.Replace('\\', '/');
        if (!viewModel.HasBlueprint(key))
        {
            reference = string.Empty;
            return false;
        }
        reference = "Data.Blueprints." + key.Replace('/', '.');
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
            LocaleService.Get("DELETE_CONFIRMATION"));
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
        if (startingDrag
            || dragStart is not Point start
            || dragSource is not ListBox list
            || dragPress is not PointerPressedEventArgs press)
            return;
        PointerPoint point = args.GetCurrentPoint(list);
        if (!point.Properties.IsLeftButtonPressed)
            return;
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
        dragStart = null;
        dragSource = null;
        dragPress = null;
    }

    private void onPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        dragStart = null;
        dragSource = null;
        dragPress = null;
    }

    private void onDragOver(object? sender, DragEventArgs args)
    {
        if (DataContext is not FileExplorerViewModel viewModel
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

    private enum ExternalOpenTarget
    {
        Folder,
        VsCode,
        Cursor,
    }
}
