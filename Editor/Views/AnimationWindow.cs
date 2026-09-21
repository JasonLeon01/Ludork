using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed class AnimationWindow : Window
{
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService projectSave;
    private readonly EditorDocument? resourceDocument;
    private readonly EditorDocumentBinding documentBinding;
    private string key => resourceDocument?.Key ?? string.Empty;
    private readonly Toast toast;

    public AnimationWindow(
        ProjectDataStore gameData,
        ProjectSaveService projectSave,
        string key,
        JsonObject data)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        resourceDocument = gameData.GetDocument("Animations", key);
        Title = $"{LocaleService.Get("ANIMATION_WINDOW")} - {key}";
        Width = 1200;
        Height = 900;
        MinWidth = 900;
        MinHeight = 640;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);
        Content = DeferredWindowInitializer.CreateLoadingContent();
        _ = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            await EditorUiBatch.YieldAsync(cancellationToken);
            Content = new AnimationEditor(gameData, key, resourceDocument?.Data ?? data);
        });
        toast = new Toast(this);
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        documentBinding = new EditorDocumentBinding(this, gameData, () => resourceDocument,
            () => $"{LocaleService.Get("ANIMATION_WINDOW")} - {this.key}", closeWhenDeleted: true);
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
            EditorFeedback.ShowHistory(toast, "Undo", documentBinding.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", documentBinding.Redo());
        else
            return;
        args.Handled = true;
    }
}
