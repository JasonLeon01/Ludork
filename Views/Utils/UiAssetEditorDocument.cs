using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

public sealed class UiAssetEditorDocument
{
    private readonly GameDataService gameData;
    private JsonObject data;
    private JsonObject? gestureStart;
    private string assetKey;

    private UiAssetEditorDocument(GameDataService gameData, string assetKey, JsonObject data)
    {
        this.gameData = gameData;
        this.assetKey = assetKey;
        this.data = (JsonObject)data.DeepClone();
    }

    public event EventHandler? Changed;

    public string AssetKey => assetKey;
    public string DocumentKey => "UiAsset:" + assetKey;
    public string Title => assetKey;
    public JsonObject Data => data;
    public bool IsGestureActive => gestureStart is not null;

    public static UiAssetEditorDocument? Create(GameDataService gameData, string key)
    {
        string normalizedKey = NormalizeKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        return gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? asset)
            ? new UiAssetEditorDocument(gameData, normalizedKey, asset)
            : null;
    }

    public static string NormalizeKey(string key)
    {
        return UiAssetSchema.NormalizeAssetKey(key);
    }

    public bool Rekey(string key)
    {
        string normalizedKey = NormalizeKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        if (normalizedKey.Length == 0
            || !gameData.UiAssetsData.ContainsKey(dataKey))
        {
            return false;
        }
        assetKey = normalizedKey;
        return true;
    }

    public bool Reload()
    {
        string dataKey = UiAssetSchema.ToAssetDataKey(assetKey);
        if (!gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? stored))
            return false;
        data = (JsonObject)stored.DeepClone();
        gestureStart = null;
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public void BeginGesture()
    {
        if (gestureStart is null)
            gestureStart = (JsonObject)data.DeepClone();
    }

    public bool CommitGesture()
    {
        if (gestureStart is null)
            return false;
        JsonObject start = gestureStart;
        gestureStart = null;
        if (JsonNode.DeepEquals(start, data))
            return false;
        return commitWorking();
    }

    public void CancelGesture()
    {
        if (gestureStart is null)
            return;
        data = gestureStart;
        gestureStart = null;
        Changed?.Invoke(this, EventArgs.Empty);
    }

    public bool Flush()
    {
        if (gestureStart is not null)
            return CommitGesture();
        return commitWorking();
    }

    public JsonObject? FindNode(string nodeName)
    {
        return findNode(getRoot(), nodeName);
    }

    public JsonObject? FindParent(string nodeName)
    {
        JsonObject? root = getRoot();
        return root is null ? null : findParent(root, nodeName);
    }

    public string? AddNode(
        string parentName,
        string controlId,
        string preferredName,
        JsonObject? defaultProperties,
        JsonObject? defaultSlot)
    {
        JsonObject? parent = FindNode(parentName);
        if (parent is null)
            return null;
        JsonArray children = ensureArray(parent, "children");
        string name = createUniqueName(preferredName);
        JsonObject node = new()
        {
            ["name"] = name,
            ["controlId"] = controlId,
            ["properties"] = defaultProperties?.DeepClone() ?? new JsonObject(),
            ["slot"] = defaultSlot?.DeepClone() ?? CreateDefaultCanvasSlot(),
            ["editor"] = new JsonObject(),
            ["children"] = new JsonArray(),
        };
        JsonObject before = (JsonObject)data.DeepClone();
        children.Add(node);
        completeMutation(before);
        return name;
    }

    public bool DeleteNode(string nodeName)
    {
        JsonObject? root = getRoot();
        JsonObject? target = FindNode(nodeName);
        if (root is null
            || target is null
            || string.Equals(getString(root, "name"), nodeName, StringComparison.Ordinal))
        {
            return false;
        }
        JsonObject before = (JsonObject)data.DeepClone();
        HashSet<string> removedNames = enumerateNodes(target)
            .Select(node => getString(node, "name"))
            .ToHashSet(StringComparer.Ordinal);
        bool removed = removeNode(root, nodeName, out _);
        if (!removed)
            return false;
        removeAnimationsForTargets(removedNames);
        completeMutation(before);
        return true;
    }

    public string? DuplicateNode(string nodeName)
    {
        JsonObject? source = FindNode(nodeName);
        JsonObject? parent = FindParent(nodeName);
        if (source is null
            || parent is null
            || parent["children"] is not JsonArray children)
        {
            return null;
        }
        int sourceIndex = children.IndexOf(source);
        if (sourceIndex < 0)
            return null;
        JsonObject copy = (JsonObject)source.DeepClone();
        HashSet<string> names = enumerateNodes()
            .Select(node => getString(node, "name"))
            .Where(name => name.Length != 0)
            .ToHashSet(StringComparer.Ordinal);
        Dictionary<string, string> replacements = new Dictionary<string, string>(StringComparer.Ordinal);
        replaceCloneNames(copy, names, replacements);
        JsonObject before = (JsonObject)data.DeepClone();
        children.Insert(sourceIndex + 1, copy);
        duplicateAnimationsForTargets(replacements);
        completeMutation(before);
        return getString(copy, "name");
    }

    public bool MoveNode(
        string nodeName,
        string parentName,
        int index,
        JsonObject? slot = null)
    {
        JsonObject? root = getRoot();
        JsonObject? destination = FindNode(parentName);
        if (root is null
            || destination is null
            || string.Equals(getString(root, "name"), nodeName, StringComparison.Ordinal)
            || string.Equals(nodeName, parentName, StringComparison.Ordinal)
            || isDescendant(FindNode(nodeName), parentName))
        {
            return false;
        }
        JsonObject before = (JsonObject)data.DeepClone();
        if (!removeNode(root, nodeName, out JsonObject? node) || node is null)
            return false;
        if (slot is not null)
            node["slot"] = slot.DeepClone();
        JsonArray children = ensureArray(destination, "children");
        int targetIndex = Math.Clamp(index, 0, children.Count);
        children.Insert(targetIndex, node);
        completeMutation(before);
        return true;
    }

    public bool RenameNode(string nodeName, string name)
    {
        JsonObject? node = FindNode(nodeName);
        string value = name.Trim();
        if (node is null
            || value.Length == 0
            || enumerateNodes()
                .Any(candidate => !ReferenceEquals(candidate, node)
                    && string.Equals(getString(candidate, "name"), value, StringComparison.Ordinal)))
        {
            return false;
        }
        JsonObject before = (JsonObject)data.DeepClone();
        node["name"] = value;
        renameAnimationTarget(nodeName, value);
        completeMutation(before);
        return true;
    }

    public bool SetNodeProperty(string nodeName, string propertyId, JsonNode? value)
    {
        JsonObject? node = FindNode(nodeName);
        if (node is null || isNestedAsset(node))
            return false;
        JsonObject properties = ensureObject(node, "properties");
        return setNodeValue(properties, propertyId, value);
    }

    public bool SetNodeEditorProperty(string nodeName, string propertyId, JsonNode? value)
    {
        JsonObject? node = FindNode(nodeName);
        if (node is null || isNestedAsset(node))
            return false;
        JsonObject editor = ensureObject(node, "editor");
        return setNodeValue(editor, propertyId, value);
    }

    public bool SetNodeSlot(string nodeName, JsonObject slot)
    {
        JsonObject? node = FindNode(nodeName);
        if (node is null || ReferenceEquals(node, getRoot()))
            return false;
        return setNodeValue(node, "slot", slot);
    }

    public bool SetDesignSize(double width, double height)
    {
        if (!double.IsFinite(width)
            || !double.IsFinite(height)
            || width <= 0
            || height <= 0)
        {
            return false;
        }
        width = Math.Round(width);
        height = Math.Round(height);
        JsonObject size = new()
        {
            ["width"] = width,
            ["height"] = height,
        };
        JsonObject before = (JsonObject)data.DeepClone();
        data["designSize"] = size;
        JsonObject? root = getRoot();
        if (root is not null
            && string.Equals(
                getString(root, "controlId"),
                "Engine.Canvas",
                StringComparison.Ordinal))
        {
            JsonObject properties = ensureObject(root, "properties");
            properties["size"] = new JsonArray(width, height);
        }
        if (JsonNode.DeepEquals(before, data))
            return false;
        completeMutation(before);
        return true;
    }

    public bool SetPalette(
        bool exposed,
        string displayName,
        string category)
    {
        JsonObject palette = new()
        {
            ["exposed"] = exposed,
            ["displayName"] = displayName.Trim(),
            ["category"] = category.Trim(),
        };
        return setNodeValue(data, "palette", palette);
    }

    public bool SetAnimations(JsonArray animations)
    {
        return setNodeValue(data, "animations", animations);
    }

    public static JsonObject CreateDefaultCanvasSlot()
    {
        return new JsonObject
        {
            ["anchors"] = new JsonObject
            {
                ["min"] = createPoint(0, 0),
                ["max"] = createPoint(0, 0),
            },
            ["offsets"] = new JsonObject
            {
                ["left"] = 0,
                ["top"] = 0,
                ["right"] = 100,
                ["bottom"] = 34,
            },
            ["alignment"] = createPoint(0, 0),
            ["autoSize"] = false,
            ["zOrder"] = 0,
        };
    }

    private bool setNodeValue(JsonObject target, string propertyName, JsonNode? value)
    {
        bool exists = target.TryGetPropertyValue(propertyName, out JsonNode? current);
        if (exists && JsonNode.DeepEquals(current, value))
            return false;
        JsonObject before = (JsonObject)data.DeepClone();
        target[propertyName] = value?.DeepClone();
        completeMutation(before);
        return true;
    }

    private void completeMutation(JsonObject before)
    {
        if (JsonNode.DeepEquals(before, data))
            return;
        if (gestureStart is not null)
            Changed?.Invoke(this, EventArgs.Empty);
        else
            commitWorking();
    }

    private bool commitWorking()
    {
        string dataKey = UiAssetSchema.ToAssetDataKey(assetKey);
        if (!gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? stored)
            || JsonNode.DeepEquals(stored, data))
        {
            return false;
        }
        gameData.UpdateUiAsset(assetKey, (JsonObject)data.DeepClone());
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    private JsonObject? getRoot()
    {
        return data["root"] as JsonObject;
    }

    private IEnumerable<JsonObject> enumerateNodes()
    {
        JsonObject? root = getRoot();
        return root is null ? [] : enumerateNodes(root);
    }

    private static IEnumerable<JsonObject> enumerateNodes(JsonObject node)
    {
        yield return node;
        if (node["children"] is not JsonArray children)
            yield break;
        foreach (JsonObject child in children.OfType<JsonObject>())
        {
            foreach (JsonObject nested in enumerateNodes(child))
                yield return nested;
        }
    }

    private string createUniqueName(string preferredName)
    {
        HashSet<string> names = enumerateNodes()
            .Select(node => getString(node, "name"))
            .Where(name => name.Length != 0)
            .ToHashSet(StringComparer.Ordinal);
        return createUniqueName(preferredName, names);
    }

    private static string createUniqueName(string preferredName, ISet<string> names)
    {
        string baseName = new string((preferredName ?? string.Empty)
            .Where(character => char.IsLetterOrDigit(character) || character == '_')
            .ToArray());
        if (baseName.Length == 0)
            baseName = "Widget";
        if (!names.Contains(baseName))
            return baseName;
        int suffix = 2;
        while (names.Contains(baseName + suffix))
            suffix++;
        return baseName + suffix;
    }

    private static void replaceCloneNames(
        JsonObject node,
        ISet<string> names,
        IDictionary<string, string> replacements)
    {
        string previous = getString(node, "name");
        string name = createUniqueName(previous, names);
        node["name"] = name;
        names.Add(name);
        replacements[previous] = name;
        if (node["children"] is not JsonArray children)
            return;
        foreach (JsonObject child in children.OfType<JsonObject>())
            replaceCloneNames(child, names, replacements);
    }

    private JsonArray animations()
    {
        return ensureArray(data, "animations");
    }

    private void renameAnimationTarget(string previous, string next)
    {
        foreach (JsonObject animation in animations().OfType<JsonObject>())
        {
            if (string.Equals(getString(animation, "target"), previous, StringComparison.Ordinal))
                animation["target"] = next;
        }
    }

    private void removeAnimationsForTargets(ISet<string> targets)
    {
        JsonArray values = animations();
        for (int index = values.Count - 1; index >= 0; index--)
        {
            if (values[index] is JsonObject animation
                && targets.Contains(getString(animation, "target")))
            {
                values.RemoveAt(index);
            }
        }
    }

    private void duplicateAnimationsForTargets(IReadOnlyDictionary<string, string> replacements)
    {
        JsonArray values = animations();
        JsonArray copies = new JsonArray();
        foreach (JsonObject animation in values.OfType<JsonObject>())
        {
            string target = getString(animation, "target");
            if (!replacements.TryGetValue(target, out string? replacement))
                continue;
            JsonObject copy = (JsonObject)animation.DeepClone();
            copy["target"] = replacement;
            copies.Add(copy);
        }
        foreach (JsonNode? copy in copies)
            values.Add(copy?.DeepClone());
    }

    private static JsonObject? findNode(JsonObject? node, string nodeName)
    {
        if (node is null)
            return null;
        if (string.Equals(getString(node, "name"), nodeName, StringComparison.Ordinal))
            return node;
        if (node["children"] is not JsonArray children)
            return null;
        foreach (JsonObject child in children.OfType<JsonObject>())
        {
            JsonObject? result = findNode(child, nodeName);
            if (result is not null)
                return result;
        }
        return null;
    }

    private static JsonObject? findParent(JsonObject node, string nodeName)
    {
        if (node["children"] is not JsonArray children)
            return null;
        foreach (JsonObject child in children.OfType<JsonObject>())
        {
            if (string.Equals(getString(child, "name"), nodeName, StringComparison.Ordinal))
                return node;
            JsonObject? result = findParent(child, nodeName);
            if (result is not null)
                return result;
        }
        return null;
    }

    private static bool removeNode(
        JsonObject parent,
        string nodeName,
        out JsonObject? removed)
    {
        JsonArray children = ensureArray(parent, "children");
        for (int index = 0; index < children.Count; index++)
        {
            if (children[index] is not JsonObject child)
                continue;
            if (string.Equals(getString(child, "name"), nodeName, StringComparison.Ordinal))
            {
                children.RemoveAt(index);
                removed = child;
                return true;
            }
            if (removeNode(child, nodeName, out removed))
                return true;
        }
        removed = null;
        return false;
    }

    private static bool isDescendant(JsonObject? node, string candidateName)
    {
        if (node is null || node["children"] is not JsonArray children)
            return false;
        foreach (JsonObject child in children.OfType<JsonObject>())
        {
            if (string.Equals(getString(child, "name"), candidateName, StringComparison.Ordinal)
                || isDescendant(child, candidateName))
            {
                return true;
            }
        }
        return false;
    }

    private static bool isNestedAsset(JsonObject node)
    {
        return getString(node, "controlId").StartsWith("Project:", StringComparison.Ordinal);
    }

    private static string getString(JsonObject node, string propertyName)
    {
        return node[propertyName]?.GetValue<string>() ?? string.Empty;
    }

    private static JsonObject ensureObject(JsonObject parent, string name)
    {
        if (parent[name] is JsonObject value)
            return value;
        JsonObject result = new();
        parent[name] = result;
        return result;
    }

    private static JsonArray ensureArray(JsonObject parent, string name)
    {
        if (parent[name] is JsonArray value)
            return value;
        JsonArray result = new();
        parent[name] = result;
        return result;
    }

    private static JsonArray createPoint(double x, double y)
    {
        return new JsonArray(x, y);
    }
}

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
