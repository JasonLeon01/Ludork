using System;

namespace Ludork.Services;

public sealed class EditorActionRouter
{
    public event EventHandler<EditorActionRequest>? ActionRequested;
    public event EventHandler<EditorDataCreationRequest>? DataCreationRequested;

    public void OpenHelp()
    {
        request(EditorActionKind.Help);
    }

    public void NewBlueprint(string? destinationPath = null, string? parentClass = null)
    {
        requestCreation(new EditorDataCreationRequest(
            EditorDataKind.Blueprint,
            destinationPath,
            parentClass));
    }

    public void NewAnimation()
    {
        NewAnimation(null);
    }

    public void NewAnimation(string? destinationPath)
    {
        requestCreation(new EditorDataCreationRequest(EditorDataKind.Animation, destinationPath));
    }

    public void NewParticle(string? destinationPath = null)
    {
        requestCreation(new EditorDataCreationRequest(EditorDataKind.Particle, destinationPath));
    }

    public void NewCurve()
    {
        NewCurve(null);
    }

    public void NewCurve(string? destinationPath)
    {
        requestCreation(new EditorDataCreationRequest(EditorDataKind.Curve, destinationPath));
    }

    public void NewTextConfig(string? initialDirectory = null)
    {
        requestCreation(new EditorDataCreationRequest(
            EditorDataKind.TextConfig,
            InitialDirectory: initialDirectory));
    }

    public void NewPlainTextConfig(string? destinationPath = null)
    {
        requestCreation(new EditorDataCreationRequest(EditorDataKind.PlainTextConfig, destinationPath));
    }
    public void NewRichTextConfig(string? destinationPath = null)
    {
        requestCreation(new EditorDataCreationRequest(EditorDataKind.RichTextConfig, destinationPath));
    }
    public void NewUiAsset(string? destinationPath = null)
    {
        requestCreation(new EditorDataCreationRequest(EditorDataKind.UiAsset, destinationPath));
    }
    public void OpenSystemConfig() => request(EditorActionKind.SystemConfig);
    public void OpenGameConfig() => request(EditorActionKind.GameConfig);
    public void OpenAnimationOverview() => request(EditorActionKind.AnimationOverview);
    public void OpenParticleOverview() => request(EditorActionKind.ParticleOverview);
    public void OpenParticle(string key) => request(EditorActionKind.Particle, key);
    public void OpenAnimation(string key) => request(EditorActionKind.Animation, key);
    public void OpenCurve(string key) => request(EditorActionKind.Curve, key);
    public void OpenTextConfig(string key) => request(EditorActionKind.TextConfig, key);
    public void OpenUiAsset(string key) => request(EditorActionKind.UiAsset, key);
    public void OpenTilesets(string? key = null) => request(EditorActionKind.Tilesets, key);
    public void OpenAutoTiles(string? key = null) => request(EditorActionKind.AutoTiles, key);
    public void OpenCommonFunctions(string? key = null) => request(EditorActionKind.CommonFunctions, key);
    public void OpenGameVariables() => request(EditorActionKind.GameVariables);
    public void OpenGeneralData(string? key = null) => request(EditorActionKind.GeneralData, key);
    public void OpenBlueprint(string reference) => request(EditorActionKind.Blueprint, reference);
    public void Undo() => request(EditorActionKind.Undo);
    public void Redo() => request(EditorActionKind.Redo);

    private void request(EditorActionKind kind, string? resourceKey = null)
    {
        ActionRequested?.Invoke(this, new EditorActionRequest(kind, resourceKey));
    }
    private void requestCreation(EditorDataCreationRequest request)
    {
        DataCreationRequested?.Invoke(this, request);
    }
}
