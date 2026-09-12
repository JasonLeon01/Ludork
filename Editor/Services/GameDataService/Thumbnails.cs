namespace Ludork.Services;

public sealed partial class GameDataService
{
    public EditorThumbnailService Thumbnails { get; } = new();
}
