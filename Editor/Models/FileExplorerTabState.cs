namespace Ludork.Models;

public sealed record FileExplorerTabState(int Number, string Path, bool IconView, double Zoom, bool SourcesExpanded);
