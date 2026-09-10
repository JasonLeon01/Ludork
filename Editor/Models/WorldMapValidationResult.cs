using System.Collections.Generic;

namespace Ludork.Models;

public sealed class WorldMapValidationResult
{
    public WorldMapValidationResult(
        IReadOnlyList<WorldMapValidationIssue> issues,
        IReadOnlyList<string> layerOrder,
        IReadOnlyList<WorldMapPlacement> placements)
    {
        Issues = issues;
        LayerOrder = layerOrder;
        Placements = placements;
    }

    public bool IsValid => Issues.Count == 0;
    public IReadOnlyList<WorldMapValidationIssue> Issues { get; }
    public IReadOnlyList<string> LayerOrder { get; }
    public IReadOnlyList<WorldMapPlacement> Placements { get; }
}
