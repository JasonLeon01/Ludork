using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed class AnimationOverviewWindow : Window
{
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly ListBox animationList = new()
    {
        ItemTemplate = HintedTextPresenter.StringItemTemplate,
    };
    private readonly ContentControl editorHost = new();
    private readonly DeferredWindowInitializer initializer;
    private string currentKey = string.Empty;
    private readonly Toast toast;
    private readonly EditorDocumentBinding documentBinding;

    public AnimationOverviewWindow(GameDataService gameData, ProjectSaveService projectSave)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        animationList.ItemTemplate = DocumentStatusPresenter.CreateTemplate(gameData, "Animations");
        Title = LocaleService.Get("ANIMATION_OVERVIEW");
        Width = 1200;
        Height = 800;
        MinWidth = 1000;
        MinHeight = 680;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);

        Grid root = new() { ColumnDefinitions = new ColumnDefinitions("240,*") };
        animationList.SelectionChanged += (_, _) => select(animationList.SelectedItem as string);
        root.Children.Add(animationList);
        Grid.SetColumn(editorHost, 1);
        root.Children.Add(editorHost);
        Content = DeferredWindowInitializer.CreateLoadingContent();
        toast = new Toast(this);
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        documentBinding = new EditorDocumentBinding(this, gameData,
            () => gameData.GetDocument("Animations", currentKey),
            () => LocaleService.Get("ANIMATION_OVERVIEW") + (currentKey.Length == 0 ? string.Empty : " - " + currentKey));
        gameData.Documents.Changed += onDocumentsChanged;
        Closed += (_, _) => gameData.Documents.Changed -= onDocumentsChanged;
        initializer = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            Content = root;
            await EditorUiBatch.YieldAsync(cancellationToken);
            refreshCore();
        });
    }

    private void onDocumentsChanged(object? sender, EventArgs args)
    {
        if (!initializer.IsInitialized)
            return;
        string[] keys = gameData.AnimationsData.Keys.OrderBy(key => key, StringComparer.Ordinal).ToArray();
        if (!keys.SequenceEqual(animationList.ItemsSource?.Cast<string>() ?? [], StringComparer.Ordinal))
        {
            currentKey = (editorHost.Content as AnimationEditor)?.Key ?? currentKey;
            refreshCore();
        }
    }

    public void refresh()
    {
        if (!initializer.IsInitialized)
            return;
        refreshCore();
    }

    private void refreshCore()
    {
        string previous = currentKey;
        string[] keys = gameData.AnimationsData.Keys.OrderBy(key => key, StringComparer.Ordinal).ToArray();
        animationList.ItemsSource = keys;
        string? selected = keys.Contains(previous, StringComparer.Ordinal) ? previous : keys.FirstOrDefault();
        animationList.SelectedItem = selected;
        select(selected);
    }

    private void select(string? key)
    {
        if (currentKey == key && editorHost.Content is AnimationEditor)
            return;
        if (string.IsNullOrWhiteSpace(key) || !gameData.AnimationsData.TryGetValue(key, out JsonObject? data))
        {
            currentKey = string.Empty;
            editorHost.Content = null;
            documentBinding.Refresh();
            return;
        }
        currentKey = key;
        documentBinding.Refresh();
        editorHost.Content = new AnimationEditor(gameData, key, data);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", documentBinding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", documentBinding.Redo());
        else
            return;
        args.Handled = true;
    }
}
