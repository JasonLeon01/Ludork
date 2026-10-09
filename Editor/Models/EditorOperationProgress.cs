namespace Ludork.Models;

public sealed record EditorOperationProgress(string Stage, string? Resource = null, int Completed = 0, int? Total = null);
