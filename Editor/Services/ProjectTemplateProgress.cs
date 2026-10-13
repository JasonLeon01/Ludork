namespace Ludork.Services;

public enum ProjectTemplateStage
{
    Downloading,
    Extracting,
    Copying,
}

public sealed record ProjectTemplateProgress(
    ProjectTemplateStage Stage,
    long CompletedBytes,
    long? TotalBytes);
