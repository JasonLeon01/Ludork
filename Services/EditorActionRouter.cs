using System;
using System.IO;

namespace Ludork.Services;

public sealed class EditorActionRouter
{
    private readonly string projectPath;

    public EditorActionRouter(string projectPath)
    {
        this.projectPath = Path.GetFullPath(projectPath);
    }

    public event EventHandler<string>? ActionRequested;
    public event EventHandler<EditorDataCreationRequest>? DataCreationRequested;

    public void OpenHelp()
    {
        request("Help");
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
    public void OpenSystemConfig() => request("SystemConfig");
    public void OpenGameConfig() => request("GameConfig");
    public void OpenAnimationOverview() => request("AnimationOverview");
    public void OpenAnimation(string key) => request($"Animation:{key}");
    public void OpenCurve(string key) => request($"Curve:{key}");
    public void OpenTextConfig(string key) => request($"TextConfig:{key}");
    public void OpenUiAsset(string key) => request($"UiAsset:{key}");
    public void OpenTilesets() => request("Tilesets");
    public void OpenCommonFunctions() => request("CommonFunctions");
    public void OpenGameVariables() => request("GameVariables");
    public void OpenGeneralData(string? key = null) => request(key is null ? "GeneralData" : $"GeneralData:{key}");
    public void OpenBlueprint(string reference) => request($"Blueprint:{reference}");
    public void Undo() => request("Undo");
    public void Redo() => request("Redo");

    private void request(string action) => ActionRequested?.Invoke(this, action);
    private void requestCreation(EditorDataCreationRequest request)
    {
        DataCreationRequested?.Invoke(this, request);
    }
}

public enum EditorDataKind
{
    Blueprint,
    Animation,
    Curve,
    TextConfig,
    PlainTextConfig,
    RichTextConfig,
    UiAsset,
}

public sealed record EditorDataCreationRequest(
    EditorDataKind Kind,
    string? DestinationPath = null,
    string? ParentClass = null,
    string? DataType = null,
    string? InitialDirectory = null);
