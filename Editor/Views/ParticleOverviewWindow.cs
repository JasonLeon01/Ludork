using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class ParticleOverviewWindow : Window
{
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService projectSave;
    private readonly UiPreviewRuntimeService runtime;
    private readonly ListBox resources = new();
    private readonly ContentControl editorHost = new();
    private readonly TextBox search = EditorInputs.CreateEditableTextBox();
    private readonly EditorDocumentBinding binding;
    private readonly Toast toast;
    private string currentKey = string.Empty;
    private bool refreshing;

    public ParticleOverviewWindow(ProjectDataStore gameData, ProjectSaveService projectSave,
        UiPreviewRuntimeService runtime, Func<Task> create)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.runtime = runtime;
        Width = 1580;
        Height = 920;
        MinWidth = 1280;
        MinHeight = 720;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);
        search.PlaceholderText = LocaleService.Get("PARTICLE_SEARCH");
        resources.ItemTemplate = DocumentStatusPresenter.CreateTemplate(gameData, "Particles");
        MenuItem copy = new() { Header = LocaleService.Get("PARTICLE_COPY") };
        MenuItem rename = new() { Header = LocaleService.Get("PARTICLE_RENAME") };
        MenuItem delete = new() { Header = LocaleService.Get("PARTICLE_DELETE") };
        copy.Click += async (_, _) => await duplicateAsync();
        rename.Click += async (_, _) => await renameAsync();
        delete.Click += async (_, _) => await deleteAsync();
        resources.ContextMenu = new ContextMenu { ItemsSource = new[] { copy, rename, delete } };
        Button add = new() { Content = LocaleService.Get("NEW_PARTICLE"), HorizontalAlignment = HorizontalAlignment.Stretch };
        add.Click += async (_, _) => await create();
        DockPanel sidebar = new() { Margin = new Thickness(8), LastChildFill = true };
        DockPanel.SetDock(search, Dock.Top);
        DockPanel.SetDock(add, Dock.Bottom);
        sidebar.Children.Add(search);
        sidebar.Children.Add(add);
        sidebar.Children.Add(resources);
        Grid root = new() { ColumnDefinitions = new ColumnDefinitions("220,5,*") };
        root.Children.Add(sidebar);
        GridSplitter splitter = new() { Width = 5, HorizontalAlignment = HorizontalAlignment.Stretch };
        Grid.SetColumn(splitter, 1);
        root.Children.Add(splitter);
        Grid.SetColumn(editorHost, 2);
        root.Children.Add(editorHost);
        Content = root;
        toast = new Toast(this);
        binding = new EditorDocumentBinding(this, gameData,
            () => gameData.GetDocument("Particles", currentKey),
            () => LocaleService.Get("PARTICLE_OVERVIEW") + (currentKey.Length == 0 ? "" : " - " + currentKey));
        resources.SelectionChanged += (_, _) => { if (!refreshing) select(resources.SelectedItem as string); };
        search.TextChanged += (_, _) => refresh();
        gameData.Documents.Changed += onDocumentsChanged;
        Closed += (_, _) => gameData.Documents.Changed -= onDocumentsChanged;
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        _ = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            await EditorUiBatch.YieldAsync(cancellationToken);
            refresh();
        });
    }

    private void onDocumentsChanged(object? sender, EventArgs args) => refresh();

    public void PausePreview() => (editorHost.Content as ParticleEditor)?.PausePreview();

    private void refresh()
    {
        string filter = search.Text?.Trim() ?? string.Empty;
        string[] keys = gameData.Assets.ParticlesData.Keys.Where(key => key.Contains(filter, StringComparison.OrdinalIgnoreCase))
            .OrderBy(key => key, StringComparer.Ordinal).ToArray();
        string previous = (editorHost.Content as ParticleEditor)?.Key ?? currentKey;
        if (keys.SequenceEqual(resources.ItemsSource?.Cast<string>() ?? [], StringComparer.Ordinal)
            && previous == currentKey)
            return;
        refreshing = true;
        resources.ItemsSource = keys;
        resources.SelectedItem = keys.Contains(previous, StringComparer.Ordinal) ? previous : keys.FirstOrDefault();
        refreshing = false;
        select(resources.SelectedItem as string);
    }

    private void select(string? key)
    {
        key ??= string.Empty;
        if (key == currentKey && editorHost.Content is ParticleEditor)
            return;
        currentKey = key;
        editorHost.Content = key.Length == 0 ? null : new ParticleEditor(gameData, runtime, key);
        binding.Refresh();
    }

    private async Task duplicateAsync()
    {
        EditorDocument? source = gameData.GetDocument("Particles", currentKey);
        if (source is null)
            return;
        string? key = await askNameAsync(LocaleService.Get("PARTICLE_COPY"), currentKey + " (copy)", false);
        if (key is null)
            return;
        string path = Path.Combine(gameData.ProjectPath, "Data", "Particles", key + ".json");
        if (!gameData.TryCopyManagedPath(source.Path, path, out string? error) || error is not null)
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), error ?? LocaleService.Get("PARTICLE_EXISTS"));
            return;
        }
        resources.SelectedItem = key;
    }

    private async Task renameAsync()
    {
        if (currentKey.Length == 0)
            return;
        string oldKey = currentKey;
        string? key = await askNameAsync(LocaleService.Get("PARTICLE_RENAME"), oldKey, true);
        if (key is null || key == oldKey)
            return;
        if (!gameData.RenameDocumentResource("Particles", oldKey, key))
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("PARTICLE_EXISTS"));
    }

    private async Task<string?> askNameAsync(string title, string initial, bool renaming)
    {
        string? key = await SingleRowDialog.ShowAsync(this, title, LocaleService.Get("PARTICLE_ENTER_NAME"),
            gameData.Assets.ParticlesData.Keys.Where(key => !renaming || key != currentKey), initial);
        if (string.IsNullOrWhiteSpace(key))
            return null;
        key = key.Trim().Replace('\\', '/');
        if (Path.IsPathRooted(key) || key.Split('/').Any(part => part is "" or "." or "..") || Path.HasExtension(key))
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), LocaleService.Get("SELECT_PARTICLE_PATH"));
            return null;
        }
        return key;
    }

    private async Task deleteAsync()
    {
        string key = currentKey;
        if (key.Length == 0 || !await ConfirmationDialog.ShowAsync(this, LocaleService.Get("CONFIRM_DELETE"),
                key + Environment.NewLine + LocaleService.Get("DELETE_DOCUMENT_CONFIRMATION")))
            return;
        await EditorResourceOperations.DeleteAsync(this, () => gameData.DeleteDocumentResource("Particles", key));
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", binding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", binding.Redo());
        else
            return;
        args.Handled = true;
    }
}
