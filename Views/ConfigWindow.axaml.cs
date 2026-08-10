using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public partial class ConfigWindow : Window
{
    private GameDataService? gameData;
    private ProjectSaveService? projectSave;
    private Toast? toast;
    public ConfigWindow()
    {
        InitializeComponent();
        Title = LocaleService.Get("SYSTEM_CONFIG");
    }

    public ConfigWindow(GameDataService gameData, ProjectSaveService projectSave) : this()
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        toast = new Toast(this);
        populate(gameData);
        gameData.DataRestored += onDataRestored;
        Closed += (_, _) => gameData.DataRestored -= onDataRestored;
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    private void onDataRestored(object? sender, System.EventArgs args)
    {
        if (gameData is null)
            return;
        PanelGrid.Children.Clear();
        PanelGrid.RowDefinitions.Clear();
        populate(gameData);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (gameData is null || projectSave is null
            || !EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast!, "Undo", gameData.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast!, "Redo", gameData.Redo());
        else
            return;
        args.Handled = true;
    }

    private void populate(GameDataService gameData)
    {
        int index = 0;
        foreach (KeyValuePair<string, JsonObject> entry in gameData.SystemConfigData)
        {
            ConfigDictPanel panel = new(this, gameData, entry.Key, entry.Value)
            {
                HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch,
                VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top,
            };
            int row = index / 2;
            int column = index % 2;
            while (PanelGrid.RowDefinitions.Count <= row)
                PanelGrid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            Grid.SetRow(panel, row);
            Grid.SetColumn(panel, column);
            PanelGrid.Children.Add(panel);
            index += 1;
        }
    }
}
