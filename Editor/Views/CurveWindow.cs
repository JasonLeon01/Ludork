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
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService projectSave;
    private readonly EditorDocument? resourceDocument;
    private readonly EditorDocumentBinding documentBinding;
    private string key => resourceDocument?.Key ?? string.Empty;
    private readonly Toast toast;

    public CurveWindow(
        ProjectDataStore gameData,
        ProjectSaveService projectSave,
        string key,
        JsonObject data)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        resourceDocument = gameData.GetDocument("Curves", key);
        Title = $"{LocaleService.Get("CURVE_WINDOW")} - {key}";
        Width = 900;
        Height = 620;
        MinWidth = 720;
        MinHeight = 500;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);
        editor = new CurveEditor(gameData, key, data);
        Content = editor;
        toast = new Toast(this);
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        documentBinding = new EditorDocumentBinding(this, gameData, () => resourceDocument,
            () => $"{LocaleService.Get("CURVE_WINDOW")} - {this.key}", closeWhenDeleted: true);
    }

    public void Reload(JsonObject data) => editor.Reload(data);

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.S)
        {
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        }
        else if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", documentBinding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", documentBinding.Redo());
        else
            return;
        args.Handled = true;
    }
}
