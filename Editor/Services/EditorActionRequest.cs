namespace Ludork.Services;

public enum EditorActionKind
{
    Help,
    SystemConfig,
    GameConfig,
    AnimationOverview,
    ParticleOverview,
    Particle,
    Subtitle,
    Animation,
    Curve,
    TextConfig,
    UiAsset,
    Tilesets,
    AutoTiles,
    CommonFunctions,
    GameVariables,
    GeneralData,
    Blueprint,
    Undo,
    Redo,
}

public sealed record EditorActionRequest(EditorActionKind Kind, string? ResourceKey = null);
