using Ludork.Models;

namespace Ludork.Views.Utils.BlueprintGraph;

internal sealed record BlueprintNodePickerRow(
    string DisplayText,
    int Depth,
    BlueprintNodePickerSelection? Selection = null,
    string? GroupPath = null,
    bool IsExpanded = false)
{
    public bool IsGroup => GroupPath is not null;
}
