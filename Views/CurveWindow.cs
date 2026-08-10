using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed class CurveWindow : Window
{
    private readonly CurveEditor editor;
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly string key;
    private readonly Toast toast;

    public CurveWindow(
        GameDataService gameData,
        ProjectSaveService projectSave,
        string key,
        JsonObject data)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.key = key;
        Title = $"{LocaleService.Get("CURVE_WINDOW")} - {key}";
        Width = 900;
        Height = 620;
        MinWidth = 720;
        MinHeight = 500;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Avalonia.Media.Brushes.Black;
        EditorWindowIcon.Apply(this);
        editor = new CurveEditor(gameData, key, data);
        Content = editor;
        toast = new Toast(this);
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        gameData.DataRestored += onDataRestored;
        Closed += (_, _) => gameData.DataRestored -= onDataRestored;
    }

    public void Reload(JsonObject data) => editor.Reload(data);

    private void onDataRestored(object? sender, System.EventArgs args)
    {
        if (gameData.CurvesData.TryGetValue(key, out JsonObject? data))
            Reload(data);
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
        {
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        }
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", gameData.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", gameData.Redo());
        else
            return;
        args.Handled = true;
    }
}
