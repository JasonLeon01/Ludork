using Ludork.Services;
using System.Collections.Generic;

namespace Ludork.Views.Utils;

public sealed class UiHierarchyItem
{
    public UiHierarchyItem(
        string nodeName,
        string name,
        string controlId,
        string controlLabel,
        bool isNestedAsset,
        bool isVisible,
        IReadOnlyList<UiHierarchyItem> children)
    {
        NodeName = nodeName;
        Name = name;
        ControlId = controlId;
        ControlLabel = controlLabel;
        IsNestedAsset = isNestedAsset;
        IsVisible = isVisible;
        Children = children;
    }

    public string NodeName { get; }
    public string Name { get; }
    public string ControlId { get; }
    public string ControlLabel { get; }
    public bool IsNestedAsset { get; }
    public bool IsVisible { get; }
    public bool CanEditVisibility => !IsNestedAsset;
    public string VisibilityLabel => LocaleService.Get("VISIBILITY");
    public IReadOnlyList<UiHierarchyItem> Children { get; }
}
