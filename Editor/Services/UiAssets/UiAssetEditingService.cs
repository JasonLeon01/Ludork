using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services.UiAssets;

public sealed class UiAssetEditingService
{
    public enum Failure
    {
        None,
        SelectContainer,
        ContainerRejectsChild,
        AssetCycle,
        UnknownControl,
    }

    public enum DropPosition
    {
        Before,
        Inside,
        After,
    }

    private readonly GameDataService gameData;
    private readonly UiControlRegistryService controlRegistry;

    public UiAssetEditingService(GameDataService gameData, UiControlRegistryService controlRegistry)
    {
        this.gameData = gameData;
        this.controlRegistry = controlRegistry;
    }

    internal bool TryCreateControl(
        UiAssetEditorDocument document,
        string? selectedNodeName,
        string controlId,
        out JsonObject? parent,
        out JsonObject? node,
        out Failure failure)
    {
        parent = null;
        node = null;
        failure = Failure.UnknownControl;
        if (!controlRegistry.IsReady)
            return false;
        IReadOnlyDictionary<string, UiControlDescriptor> controls = controlRegistry.CreateControlLookup();
        if (!controls.TryGetValue(controlId, out UiControlDescriptor? descriptor))
            return false;
        parent = getAddParent(document, selectedNodeName, controls);
        if (parent is null)
        {
            failure = Failure.SelectContainer;
            return false;
        }
        if (!canAcceptChild(parent, null, controls))
        {
            failure = Failure.ContainerRejectsChild;
            return false;
        }
        UiAssetDependencyGraph graph = new(gameData.UiAssetsData, document.AssetKey, document.Data);
        if (!canInsertControl(document, descriptor, graph, out failure))
            return false;
        JsonObject properties = new();
        foreach (UiControlPropertyDescriptor property in descriptor.Properties)
        {
            if (!property.EditorOnly && property.Default is not null)
                properties[property.Id] = property.Default.DeepClone();
        }
        node = new JsonObject
        {
            ["name"] = descriptor.DisplayName,
            ["controlId"] = descriptor.ControlId,
            ["properties"] = properties,
            ["slot"] = createSlot(parent, controls),
            ["editor"] = new JsonObject(),
            ["children"] = new JsonArray(),
        };
        failure = Failure.None;
        return true;
    }

    internal bool CanDuplicateNode(UiAssetEditorDocument document, string nodeName)
    {
        if (!controlRegistry.IsReady
            || !tryGetNodeLocation(document, nodeName, out JsonObject? parent, out _, out _))
        {
            return false;
        }
        JsonObject? source = document.FindNode(nodeName);
        IReadOnlyDictionary<string, UiControlDescriptor> controls = controlRegistry.CreateControlLookup();
        if (source is null || parent is null || !canAcceptChild(parent, null, controls))
            return false;
        UiAssetDependencyGraph graph = new(gameData.UiAssetsData, document.AssetKey, document.Data);
        return canCopySubtree(document, source, controls, graph);
    }

    internal bool TryGetMoveLocation(
        UiAssetEditorDocument document,
        string nodeName,
        string parentName,
        int index,
        out int targetIndex)
    {
        targetIndex = 0;
        if (!controlRegistry.IsReady
            || !tryGetNodeLocation(document, nodeName, out JsonObject? sourceParent, out _, out int sourceIndex))
        {
            return false;
        }
        JsonObject? source = document.FindNode(nodeName);
        JsonObject? destination = document.FindNode(parentName);
        if (source is null
            || destination is null
            || ReferenceEquals(source, destination)
            || isDescendant(source, parentName)
            || !canAcceptChild(destination, nodeName, controlRegistry.CreateControlLookup()))
        {
            return false;
        }
        bool sameParent = ReferenceEquals(sourceParent, destination);
        int childCount = destination["children"] is JsonArray children ? children.Count : 0;
        targetIndex = Math.Clamp(index, 0, childCount - (sameParent ? 1 : 0));
        return !sameParent || targetIndex != sourceIndex;
    }

    internal bool TryGetSiblingMoveLocation(
        UiAssetEditorDocument document,
        string nodeName,
        int direction,
        out string parentName,
        out int index)
    {
        parentName = string.Empty;
        index = 0;
        if (!tryGetNodeLocation(document, nodeName, out JsonObject? parent, out JsonArray? siblings, out int sourceIndex)
            || parent is null
            || siblings is null
            || direction is not (-1 or 1))
        {
            return false;
        }
        index = sourceIndex + direction;
        parentName = getString(parent, "name");
        return index >= 0
            && index < siblings.Count
            && TryGetMoveLocation(document, nodeName, parentName, index, out index);
    }

    internal bool TryGetIndentLocation(
        UiAssetEditorDocument document,
        string nodeName,
        out string parentName,
        out int index)
    {
        parentName = string.Empty;
        index = 0;
        if (!tryGetNodeLocation(document, nodeName, out _, out JsonArray? siblings, out int sourceIndex)
            || siblings is null
            || sourceIndex <= 0
            || siblings[sourceIndex - 1] is not JsonObject destination)
        {
            return false;
        }
        parentName = getString(destination, "name");
        index = destination["children"] is JsonArray children ? children.Count : 0;
        return TryGetMoveLocation(document, nodeName, parentName, index, out index);
    }

    internal bool TryGetOutdentLocation(
        UiAssetEditorDocument document,
        string nodeName,
        out string parentName,
        out int index)
    {
        parentName = string.Empty;
        index = 0;
        if (!tryGetNodeLocation(document, nodeName, out JsonObject? parent, out _, out _)
            || parent is null
            || !tryGetNodeLocation(document, getString(parent, "name"), out JsonObject? grandParent, out _, out int parentIndex)
            || grandParent is null)
        {
            return false;
        }
        parentName = getString(grandParent, "name");
        index = parentIndex + 1;
        return TryGetMoveLocation(document, nodeName, parentName, index, out index);
    }

    internal bool TryGetDropLocation(
        UiAssetEditorDocument document,
        string nodeName,
        string targetNodeName,
        DropPosition position,
        out string parentName,
        out int index)
    {
        parentName = string.Empty;
        index = 0;
        JsonObject? source = document.FindNode(nodeName);
        JsonObject? target = document.FindNode(targetNodeName);
        if (!controlRegistry.IsReady
            || source is null
            || target is null
            || ReferenceEquals(source, target)
            || isDescendant(source, targetNodeName)
            || !tryGetNodeLocation(document, nodeName, out JsonObject? sourceParent, out _, out int sourceIndex))
        {
            return false;
        }
        IReadOnlyDictionary<string, UiControlDescriptor> controls = controlRegistry.CreateControlLookup();
        if (!tryGetDropLocation(document, target, nodeName, position, controls, out parentName, out index))
        {
            return false;
        }
        if (sourceParent is not null
            && string.Equals(getString(sourceParent, "name"), parentName, StringComparison.Ordinal)
            && sourceIndex < index)
        {
            index--;
        }
        return TryGetMoveLocation(document, nodeName, parentName, index, out index);
    }

    internal bool TryGetControlDropLocation(
        UiAssetEditorDocument document,
        string controlId,
        string targetNodeName,
        DropPosition position,
        out string parentName,
        out int index,
        out Failure failure)
    {
        parentName = string.Empty;
        index = 0;
        failure = Failure.UnknownControl;
        IReadOnlyDictionary<string, UiControlDescriptor> controls = controlRegistry.CreateControlLookup();
        if (!controlRegistry.IsReady || !controls.TryGetValue(controlId, out UiControlDescriptor? descriptor))
            return false;
        JsonObject? target = document.FindNode(targetNodeName);
        failure = Failure.SelectContainer;
        if (target is null)
            return false;
        failure = Failure.ContainerRejectsChild;
        if (!tryGetDropLocation(document, target, null, position, controls, out parentName, out index))
        {
            return false;
        }
        UiAssetDependencyGraph graph = new(gameData.UiAssetsData, document.AssetKey, document.Data);
        return canInsertControl(document, descriptor, graph, out failure);
    }

    private static bool tryGetDropLocation(
        UiAssetEditorDocument document,
        JsonObject target,
        string? movingNodeName,
        DropPosition position,
        IReadOnlyDictionary<string, UiControlDescriptor> controls,
        out string parentName,
        out int index)
    {
        parentName = string.Empty;
        index = 0;
        if (position == DropPosition.Inside && canAcceptChild(target, movingNodeName, controls))
        {
            parentName = getString(target, "name");
            index = target["children"] is JsonArray children ? children.Count : 0;
            return true;
        }
        if (!tryGetNodeLocation(document, getString(target, "name"), out JsonObject? parent, out _, out int targetIndex)
            || parent is null || !canAcceptChild(parent, movingNodeName, controls))
        {
            return false;
        }
        parentName = getString(parent, "name");
        index = targetIndex + (position == DropPosition.After ? 1 : 0);
        return true;
    }

    public static JsonObject CreateDefaultCanvasSlot()
    {
        return new JsonObject
        {
            ["anchors"] = new JsonObject
            {
                ["min"] = new JsonArray(0, 0),
                ["max"] = new JsonArray(0, 0),
            },
            ["offsets"] = new JsonObject
            {
                ["left"] = 0,
                ["top"] = 0,
                ["right"] = 100,
                ["bottom"] = 34,
            },
            ["alignment"] = new JsonArray(0, 0),
            ["autoSize"] = false,
            ["zOrder"] = 0,
        };
    }

    private static JsonObject? getAddParent(
        UiAssetEditorDocument document,
        string? selectedNodeName,
        IReadOnlyDictionary<string, UiControlDescriptor> controls)
    {
        if (selectedNodeName is not null)
        {
            JsonObject? selected = document.FindNode(selectedNodeName);
            if (selected is not null && canAcceptChild(selected, null, controls))
                return selected;
            JsonObject? parent = document.FindParent(selectedNodeName);
            if (parent is not null && canAcceptChild(parent, null, controls))
                return parent;
        }
        return document.Data["root"] as JsonObject;
    }

    private static bool canAcceptChild(
        JsonObject parent,
        string? movingNodeName,
        IReadOnlyDictionary<string, UiControlDescriptor> controls)
    {
        string controlId = getString(parent, "controlId");
        if (controlId.StartsWith(UiAssetSchema.ProjectControlPrefix, StringComparison.Ordinal)
            || !controls.TryGetValue(controlId, out UiControlDescriptor? descriptor))
        {
            return false;
        }
        if (descriptor.ChildPolicy == "multiple")
            return true;
        if (descriptor.ChildPolicy != "single")
            return false;
        return parent["children"] is not JsonArray children
            || children.All(child => child is JsonObject value
                && movingNodeName is not null
                && string.Equals(getString(value, "name"), movingNodeName, StringComparison.Ordinal));
    }

    private static bool canInsertControl(
        UiAssetEditorDocument document,
        UiControlDescriptor descriptor,
        UiAssetDependencyGraph graph,
        out Failure failure)
    {
        failure = Failure.None;
        if (!descriptor.ControlId.StartsWith(UiAssetSchema.ProjectControlPrefix, StringComparison.Ordinal))
            return true;
        if (!UiAssetSchema.TryGetProjectAssetKey(descriptor.ControlId, out string targetKey))
        {
            failure = Failure.UnknownControl;
            return false;
        }
        if (!graph.TryGetAsset(targetKey, out JsonObject? target)
            || target?["palette"] is not JsonObject palette
            || palette["exposed"] is not JsonValue exposedValue
            || !exposedValue.TryGetValue<bool>(out bool exposed)
            || !exposed)
        {
            failure = Failure.UnknownControl;
            return false;
        }
        if (graph.WouldCreateCycle(document.AssetKey, targetKey))
        {
            failure = Failure.AssetCycle;
            return false;
        }
        return true;
    }

    private bool canCopySubtree(
        UiAssetEditorDocument document,
        JsonObject node,
        IReadOnlyDictionary<string, UiControlDescriptor> controls,
        UiAssetDependencyGraph graph)
    {
        if (!controls.TryGetValue(getString(node, "controlId"), out UiControlDescriptor? descriptor)
            || !canInsertControl(document, descriptor, graph, out _))
        {
            return false;
        }
        JsonArray? children = node["children"] as JsonArray;
        int childCount = children?.Count ?? 0;
        if (descriptor.ChildPolicy == "none" && childCount != 0
            || descriptor.ChildPolicy == "single" && childCount > 1
            || descriptor.ChildPolicy is not ("none" or "single" or "multiple"))
        {
            return false;
        }
        if (descriptor.ControlId.StartsWith(UiAssetSchema.ProjectControlPrefix, StringComparison.Ordinal)
            && (node["properties"] is JsonObject { Count: > 0 }
                || node["editor"] is JsonObject { Count: > 0 }))
        {
            return false;
        }
        return children is null
            || children.All(child => child is JsonObject value && canCopySubtree(document, value, controls, graph));
    }

    private static JsonObject createSlot(
        JsonObject parent,
        IReadOnlyDictionary<string, UiControlDescriptor> controls)
    {
        return controls.TryGetValue(getString(parent, "controlId"), out UiControlDescriptor? descriptor)
            && descriptor.SlotType == "canvas"
                ? CreateDefaultCanvasSlot()
                : new JsonObject();
    }

    private static bool tryGetNodeLocation(
        UiAssetEditorDocument document,
        string nodeName,
        out JsonObject? parent,
        out JsonArray? siblings,
        out int index)
    {
        parent = document.FindParent(nodeName);
        siblings = parent?["children"] as JsonArray;
        JsonObject? source = document.FindNode(nodeName);
        index = source is null || siblings is null ? -1 : siblings.IndexOf(source);
        return index >= 0;
    }

    private static bool isDescendant(JsonObject node, string nodeName)
    {
        return node["children"] is JsonArray children
            && children.OfType<JsonObject>().Any(child =>
                string.Equals(getString(child, "name"), nodeName, StringComparison.Ordinal)
                || isDescendant(child, nodeName));
    }

    private static string getString(JsonObject node, string propertyName)
    {
        return node[propertyName] is JsonValue value && value.TryGetValue<string>(out string? text)
            ? text ?? string.Empty
            : string.Empty;
    }
}
