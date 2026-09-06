using System.Collections.Generic;

namespace Ludork.ViewModels;

public sealed class ActorOutlinerItemViewModel(
    string name,
    string description,
    string layerName,
    int? actorIndex,
    IReadOnlyList<ActorOutlinerItemViewModel> children)
{
    public string Name { get; } = name;
    public string Description { get; } = description;
    public string LayerName { get; } = layerName;
    public int? ActorIndex { get; } = actorIndex;
    public IReadOnlyList<ActorOutlinerItemViewModel> Children { get; } = children;
}
