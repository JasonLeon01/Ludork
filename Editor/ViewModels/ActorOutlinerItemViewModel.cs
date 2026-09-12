using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace Ludork.ViewModels;

public sealed class ActorOutlinerItemViewModel(
    string name,
    string description,
    string layerName,
    int? actorIndex,
    IReadOnlyList<ActorOutlinerItemViewModel> children,
    string? runtimeId = null) : ViewModelBase
{
    private string name = name;
    private string description = description;
    private string layerName = layerName;
    private int? actorIndex = actorIndex;

    public string Name { get => name; private set => SetProperty(ref name, value); }
    public string Description { get => description; private set => SetProperty(ref description, value); }
    public string LayerName { get => layerName; private set => SetProperty(ref layerName, value); }
    public int? ActorIndex { get => actorIndex; private set => SetProperty(ref actorIndex, value); }
    public string? RuntimeId { get; } = runtimeId;
    public ObservableCollection<ActorOutlinerItemViewModel> Children { get; } = new(children);

    public IEnumerable<ActorOutlinerItemViewModel> EnumerateDescendants()
    {
        foreach (ActorOutlinerItemViewModel child in Children)
        {
            yield return child;
            foreach (ActorOutlinerItemViewModel descendant in child.EnumerateDescendants())
                yield return descendant;
        }
    }

    public void Update(string nextName, string nextDescription, string nextLayerName, int nextActorIndex)
    {
        Name = nextName;
        Description = nextDescription;
        LayerName = nextLayerName;
        ActorIndex = nextActorIndex;
    }
}
