using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed record MaterialPreviewFrame(
    int Width,
    int Height,
    int Stride,
    byte[] Pixels,
    long ConnectionGeneration,
    long MaterialRevision,
    long DiagnosticsRevision,
    JsonArray Diagnostics,
    string GeneratedShader);
