using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;

namespace Ludork.Services.UiAssets;

public sealed class UiAssetEditorDocument : IDisposable
{
    private static readonly ConditionalWeakTable<EditorDocument, GestureOwnership> gestureOwners = new();
    private readonly GameDataService gameData;
    private readonly UiAssetEditingService editing;
    private JsonObject data;
    private JsonObject? gestureStart;
    private long gestureId;
    private readonly EditorDocument? resourceDocument;
    private readonly string initialAssetKey;
    private string assetKey => resourceDocument is null ? initialAssetKey : UiAssetSchema.ToLogicalAssetKey(resourceDocument.Key);
    private JsonObject sourceData;
    private bool committing;

    private UiAssetEditorDocument(
        GameDataService gameData,
        UiControlRegistryService controlRegistry,
        string assetKey,
        JsonObject data)
    {
        this.gameData = gameData;
        editing = new UiAssetEditingService(gameData, controlRegistry);
        initialAssetKey = assetKey;
        resourceDocument = gameData.GetDocument("UI", UiAssetSchema.ToAssetDataKey(assetKey));
        this.data = (JsonObject)data.DeepClone();
        sourceData = (JsonObject)data.DeepClone();
        if (resourceDocument is not null)
            resourceDocument.Changed += onResourceChanged;
        gameData.Documents.Changed += onRegistryChanged;
    }

    public event EventHandler? Changed;

    public EditorDocument? ResourceDocument => resourceDocument;
    public string AssetKey => assetKey;
    public string DocumentKey => "UiAsset:" + assetKey;
    public string Title => assetKey;
    public JsonObject Data => data;
    public bool IsGestureActive => gestureStart is not null && gameData.IsHistoryGestureActive(gestureId);

    public static UiAssetEditorDocument? Create(
        GameDataService gameData,
        UiControlRegistryService controlRegistry,
        string key)
    {
        string normalizedKey = NormalizeKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        return gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? asset)
            ? new UiAssetEditorDocument(gameData, controlRegistry, normalizedKey, asset)
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
        return string.Equals(assetKey, normalizedKey, StringComparison.Ordinal);
    }

    public bool Reload()
    {
        endGesture();
        string dataKey = UiAssetSchema.ToAssetDataKey(assetKey);
        if (!gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? stored))
            return false;
        data = (JsonObject)stored.DeepClone();
        sourceData = (JsonObject)stored.DeepClone();
        gestureStart = null;
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public void BeginGesture()
    {
        finishOtherGesture();
        if (IsGestureActive)
            return;
        gestureStart = (JsonObject)data.DeepClone();
        gestureId = gameData.BeginHistoryGesture();
        if (resourceDocument is not null)
            gestureOwners.GetOrCreateValue(resourceDocument).Owner = this;
    }

    public bool CommitGesture()
    {
        bool changed = gestureStart is not null && !JsonNode.DeepEquals(gestureStart, data);
        endGesture();
        return changed;
    }

    public void CancelGesture()
    {
        if (IsGestureActive && gestureStart is JsonObject start)
        {
            data = start;
            commitWorking();
        }
        endGesture();
    }

    public bool Flush()
    {
        bool changed = commitWorking();
        return CommitGesture() || changed;
    }

    private void finishOtherGesture()
    {
        if (resourceDocument is not null
            && gestureOwners.TryGetValue(resourceDocument, out GestureOwnership? ownership)
            && ownership.Owner is UiAssetEditorDocument owner && !ReferenceEquals(owner, this))
        {
            owner.CommitGesture();
        }
    }

    private void endGesture()
    {
        gameData.EndHistoryGesture(gestureId);
        gestureId = 0;
        gestureStart = null;
        if (resourceDocument is not null && gestureOwners.TryGetValue(resourceDocument, out GestureOwnership? ownership)
            && ReferenceEquals(ownership.Owner, this))
        {
            ownership.Owner = null;
        }
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

    public string? AddControl(
        string? selectedNodeName,
        string controlId,
        out UiAssetEditingService.Failure failure)
    {
        if (!editing.TryCreateControl(
                this,
                selectedNodeName,
                controlId,
                out JsonObject? parent,
                out JsonObject? node,
                out failure)
            || parent is null
            || node is null)
        {
            return null;
        }
        string name = createUniqueName(getString(node, "name"));
        node["name"] = name;
        JsonObject before = (JsonObject)data.DeepClone();
        JsonArray children = ensureArray(parent, "children");
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
        if (!CanDuplicateNode(nodeName))
            return null;
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

    public bool CanDuplicateNode(string nodeName)
    {
        return editing.CanDuplicateNode(this, nodeName);
    }

    public bool MoveNode(string nodeName, string parentName, int index)
    {
        if (!editing.TryGetMoveLocation(this, nodeName, parentName, index, out int targetIndex))
        {
            return false;
        }
        JsonObject node = FindNode(nodeName)!;
        JsonObject sourceParent = FindParent(nodeName)!;
        JsonObject destination = FindNode(parentName)!;
        JsonArray sourceChildren = (JsonArray)sourceParent["children"]!;
        bool changedParent = !ReferenceEquals(sourceParent, destination);
        JsonObject? slot = changedParent ? editing.CreateSlot(destination) : null;
        JsonObject before = (JsonObject)data.DeepClone();
        sourceChildren.Remove(node);
        if (slot is not null)
            node["slot"] = slot;
        JsonArray children = ensureArray(destination, "children");
        children.Insert(targetIndex, node);
        completeMutation(before);
        return true;
    }

    public bool MoveWithinParent(string nodeName, int direction)
    {
        return editing.TryGetSiblingMoveLocation(this, nodeName, direction, out string parentName, out int index)
            && MoveNode(nodeName, parentName, index);
    }

    public bool IndentNode(string nodeName)
    {
        return editing.TryGetIndentLocation(this, nodeName, out string parentName, out int index)
            && MoveNode(nodeName, parentName, index);
    }

    public bool OutdentNode(string nodeName)
    {
        return editing.TryGetOutdentLocation(this, nodeName, out string parentName, out int index)
            && MoveNode(nodeName, parentName, index);
    }

    public bool TryGetDropLocation(
        string nodeName,
        string targetNodeName,
        UiAssetEditingService.DropPosition position,
        out string parentName,
        out int index)
    {
        return editing.TryGetDropLocation(this, nodeName, targetNodeName, position, out parentName, out index);
    }

    public bool RenameNode(string nodeName, string name)
    {
        JsonObject? node = FindNode(nodeName);
        string value = name.Trim();
        if (node is null
            || value.Length == 0
            || string.Equals(getString(node, "name"), value, StringComparison.Ordinal)
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
        finishOtherGesture();
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
        committing = true;
        try
        {
            gameData.UpdateUiAsset(assetKey, (JsonObject)data.DeepClone());
            sourceData = resourceDocument?.Data ?? (JsonObject)data.DeepClone();
        }
        finally
        {
            committing = false;
        }
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public void Dispose()
    {
        endGesture();
        gameData.Documents.Changed -= onRegistryChanged;
        if (resourceDocument is not null)
            resourceDocument.Changed -= onResourceChanged;
    }

    private void onResourceChanged(object? sender, EventArgs args)
    {
        if (committing || JsonNode.DeepEquals(sourceData, resourceDocument?.Data))
            return;
        if (!Reload())
        {
            data = [];
            sourceData = [];
            Changed?.Invoke(this, EventArgs.Empty);
        }
    }

    private void onRegistryChanged(object? sender, EventArgs args)
    {
        if (!committing && gestureStart is not null)
            endGesture();
    }

    private sealed class GestureOwnership
    {
        public UiAssetEditorDocument? Owner { get; set; }
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
        if (isNestedAsset(node) || node["children"] is not JsonArray children)
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
        if (isNestedAsset(node) || node["children"] is not JsonArray children)
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
        if (isNestedAsset(parent) || parent["children"] is not JsonArray children)
        {
            removed = null;
            return false;
        }
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
}
