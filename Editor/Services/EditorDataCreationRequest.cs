namespace Ludork.Services;

public enum EditorDataKind
{
    Blueprint,
    Animation,
    Particle,
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
