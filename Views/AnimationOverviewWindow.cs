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

    public AnimationOverviewWindow(GameDataService gameData, ProjectSaveService projectSave)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        Title = LocaleService.Get("ANIMATION_OVERVIEW");
        Width = 1200;
        Height = 800;
        MinWidth = 1000;
        MinHeight = 680;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Avalonia.Media.Brushes.Black;
        EditorWindowIcon.Apply(this);

        Grid root = new() { ColumnDefinitions = new ColumnDefinitions("240,*") };
        animationList.SelectionChanged += (_, _) => select(animationList.SelectedItem as string);
        root.Children.Add(animationList);
        Grid.SetColumn(editorHost, 1);
        root.Children.Add(editorHost);
        Content = DeferredWindowInitializer.CreateLoadingContent();
        toast = new Toast(this);
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        gameData.DataRestored += onDataRestored;
        Closed += (_, _) => gameData.DataRestored -= onDataRestored;
        initializer = new DeferredWindowInitializer(this, () =>
        {
            Content = root;
            refreshCore();
        });
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        currentKey = string.Empty;
        refresh();
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
        if (string.IsNullOrWhiteSpace(key) || !gameData.AnimationsData.TryGetValue(key, out JsonObject? data))
{
            currentKey = string.Empty;
            editorHost.Content = null;
            return;
        }
        if (currentKey == key && editorHost.Content is AnimationEditor)
            return;
        currentKey = key;
        editorHost.Content = new AnimationEditor(gameData, key, data);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", gameData.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", gameData.Redo());
        else
            return;
        args.Handled = true;
    }
}
