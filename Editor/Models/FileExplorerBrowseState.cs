namespace Ludork.Models;

public sealed record FileExplorerPresentationState(
    string[] SelectedPaths,
    string? PrimaryPath,
    string? SelectionAnchorPath,
    string? ScrollAnchorPath,
    double ScrollAnchorFraction,
    double HorizontalOffset,
    double VerticalOffset)
{
    public static FileExplorerPresentationState Empty { get; } = new([], null, null, null, 0, 0, 0);
}

internal sealed record FileExplorerBrowseState(
    string Path,
    string SearchText,
    bool IconView,
    double Zoom,
    bool SourcesExpanded,
    string[] ExpandedDirectories,
    FileExplorerPresentationState Presentation);
