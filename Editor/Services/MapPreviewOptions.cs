namespace Ludork.Services;

public sealed record MapPreviewOptions(
    bool Lit = false,
    bool Realtime = true,
    bool ShowFog = true,
    bool ShowPanorama = true,
    bool ShowActorOrigins = true,
    bool ShowLightBounds = false);
