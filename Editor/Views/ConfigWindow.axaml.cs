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

public partial class ConfigWindow : Window
{
    private GameDataService? gameData;
    private ProjectSaveService? projectSave;
    private Toast? toast;
    private string? activeConfigKey;
    private EditorDocumentBinding? documentBinding;
    private bool refreshingList;
    public ConfigWindow()
    {
        InitializeComponent();
        Title = LocaleService.Get("SYSTEM_CONFIG");
        EmptyState.Text = LocaleService.Get("SYSTEM_CONFIG_EMPTY");
    }

    public ConfigWindow(GameDataService gameData, ProjectSaveService projectSave) : this()
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        toast = new Toast(this);
        ConfigList.ItemTemplate = DocumentStatusPresenter.CreateTemplate(gameData, "Configs");
        documentBinding = new EditorDocumentBinding(this, gameData,
            () => activeConfigKey is null ? null : gameData.GetDocument("Configs", activeConfigKey),
            () => LocaleService.Get("SYSTEM_CONFIG") + (activeConfigKey is null ? string.Empty : " - " + activeConfigKey));

        populate();
        gameData.DataReloaded += onDataReloaded;
        Closed += onClosed;
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    private void onDataReloaded(object? sender, EventArgs args) => populate();

    private void onClosed(object? sender, EventArgs args)
    {
        if (gameData is not null)
            gameData.DataReloaded -= onDataReloaded;
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (gameData is null || projectSave is null
            || !EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast!, "Undo", documentBinding!.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast!, "Redo", documentBinding!.Redo());
        else
            return;
        args.Handled = true;
    }

    private void populate()
    {
        if (gameData is null)
            return;
        string[] keys = gameData.SystemConfigData.Keys.ToArray();
        refreshingList = true;
        ConfigList.ItemsSource = keys;
        ConfigList.SelectedItem = activeConfigKey is not null && keys.Contains(activeConfigKey, StringComparer.Ordinal)
            ? activeConfigKey : keys.FirstOrDefault();
        refreshingList = false;
        showSelectedConfig();
    }

    private void onConfigSelected(object? sender, SelectionChangedEventArgs args)
    {
        if (!refreshingList && gameData is not null)
            showSelectedConfig();
    }

    private void showSelectedConfig()
    {
        activeConfigKey = ConfigList.SelectedItem as string;
        ConfigContent.Content = gameData is not null && activeConfigKey is not null
            && gameData.SystemConfigData.TryGetValue(activeConfigKey, out JsonObject? data)
                ? new ConfigDictPanel(this, gameData, activeConfigKey, data)
                {
                    Background = EditorTheme.Brush("Background"),
                    BorderThickness = new Thickness(0),
                    Padding = new Thickness(0),
                    HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch,
                    VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top,
                }
                : null;
        ConfigScroll.Offset = default;
        EmptyState.IsVisible = ConfigContent.Content is null;
        documentBinding?.Refresh();
    }
}
