using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;

namespace Ludork.Views;

public sealed class ParticleWindow : Window
{
    private readonly EditorDocument? document;
    private readonly ProjectSaveService projectSave;
    private readonly EditorDocumentBinding binding;
    private readonly Toast toast;

    public ParticleWindow(ProjectDataStore gameData, ProjectSaveService projectSave,
        UiPreviewRuntimeService runtime, string key)
    {
        this.projectSave = projectSave;
        document = gameData.GetDocument("Particles", key);
        Width = 1440;
        Height = 900;
        MinWidth = 1080;
        MinHeight = 720;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);
        Content = DeferredWindowInitializer.CreateLoadingContent();
        _ = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            await EditorUiBatch.YieldAsync(cancellationToken);
            Content = new ParticleEditor(gameData, runtime, key);
        });
        toast = new Toast(this);
        binding = new EditorDocumentBinding(this, gameData, () => document,
            () => LocaleService.Get("PARTICLE_EDITOR") + " - " + Key, closeWhenDeleted: true);
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public string Key => document?.Key ?? string.Empty;
    public void PausePreview() => (Content as ParticleEditor)?.PausePreview();

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Avalonia.Input.Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else if (args.Key == Avalonia.Input.Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", binding.Undo());
        else if (args.Key == Avalonia.Input.Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", binding.Redo());
        else
            return;
        args.Handled = true;
    }
}
