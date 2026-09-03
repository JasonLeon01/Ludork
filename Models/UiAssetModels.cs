using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record UiDesignSize(
    double Width,
    double Height,
    string? WidthJson = null,
    string? HeightJson = null
);

public sealed record UiControlPropertyDescriptor(
    string Id,
    string DisplayName,
    string Type,
    bool Required,
    JsonNode? Default,
    bool EditorOnly = false
);

public sealed record UiControlDescriptor(
    string ControlId,
    string Source,
    string DisplayName,
    string Category,
    string? Adapter,
    string ChildPolicy,
    string? SlotType,
    IReadOnlyList<UiControlPropertyDescriptor> Properties,
    string? AssetKey = null,
    UiDesignSize? DesignSize = null
);

public sealed record UiValidationIssue(
    string Code,
    string Path,
    string Message
);

public sealed record UiAssetValidationResult(
    string AssetKey,
    IReadOnlyList<UiValidationIssue> Issues
)
{
    public bool IsValid => Issues.Count == 0;
    public IReadOnlyList<string> Errors => Issues
        .Select(issue => issue.Path.Length == 0
            ? issue.Message
            : $"{issue.Path}: {issue.Message}")
        .ToArray();
}

public static class UiAssetSchema
{
    public const string UiAssetType = "uiAsset";
    public const string AssetDataPrefix = "Assets/";
    public const string ProjectControlPrefix = "Project:";

    public static string NormalizeAssetKey(string key)
    {
        if (string.IsNullOrWhiteSpace(key)
            || !string.Equals(key, key.Trim(), StringComparison.Ordinal)
            || key.Contains('\\')
            || key.Contains(':')
            || key.StartsWith("/", StringComparison.Ordinal)
            || key.EndsWith("/", StringComparison.Ordinal)
            || key.Contains("//", StringComparison.Ordinal)
            || Path.GetFileName(key).Contains('.')
            || HasPrefix(key, "Assets")
            || HasPrefix(key, "UI/Assets")
            || HasPrefix(key, "Data/UI/Assets"))
        {
            return string.Empty;
        }
        string[] parts = key.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length == 0
            || parts.Any(part => part is "." or ".."
                || part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0))
        {
            return string.Empty;
        }
        return string.Join('/', parts);
    }

    private static bool HasPrefix(string key, string prefix)
    {
        return string.Equals(key, prefix, StringComparison.Ordinal)
            || key.StartsWith(prefix + "/", StringComparison.Ordinal);
    }

    public static string ToAssetDataKey(string key)
    {
        string normalized = NormalizeAssetKey(key);
        return normalized.Length == 0 ? string.Empty : AssetDataPrefix + normalized;
    }

    public static string ToLogicalAssetKey(string key)
    {
        if (!key.StartsWith(AssetDataPrefix, StringComparison.Ordinal))
            return string.Empty;
        string logicalKey = key[AssetDataPrefix.Length..];
        string normalized = NormalizeAssetKey(logicalKey);
        return string.Equals(normalized, logicalKey, StringComparison.Ordinal)
            ? logicalKey
            : string.Empty;
    }

    public static string ToProjectControlId(string key)
    {
        string normalized = NormalizeAssetKey(key);
        return normalized.Length == 0 ? string.Empty : ProjectControlPrefix + normalized;
    }

    public static bool TryGetProjectAssetKey(string controlId, out string assetKey)
    {
        assetKey = string.Empty;
        if (!controlId.StartsWith(ProjectControlPrefix, StringComparison.Ordinal))
            return false;
        string logicalKey = controlId[ProjectControlPrefix.Length..];
        string normalized = NormalizeAssetKey(logicalKey);
        if (!string.Equals(normalized, logicalKey, StringComparison.Ordinal))
        {
            return false;
        }
        assetKey = logicalKey;
        return true;
    }

    public static JsonObject CreateDefaultAsset(
        string key,
        double width = 640.0,
        double height = 480.0)
    {
        string normalizedKey = NormalizeAssetKey(key);
        string displayName = normalizedKey.Length == 0
            ? "UI Asset"
            : Path.GetFileName(normalizedKey);
        return new JsonObject
        {
            ["type"] = UiAssetType,
            ["designSize"] = new JsonObject
            {
                ["width"] = width,
                ["height"] = height,
            },
            ["palette"] = new JsonObject
            {
                ["exposed"] = true,
                ["displayName"] = displayName,
                ["category"] = "Project",
            },
            ["animations"] = new JsonArray(),
            ["root"] = new JsonObject
            {
                ["name"] = "Root",
                ["controlId"] = "Engine.Canvas",
                ["properties"] = new JsonObject
                {
                    ["size"] = new JsonArray(
                        (long)Math.Max(0.0, Math.Ceiling(width)),
                        (long)Math.Max(0.0, Math.Ceiling(height))),
                    ["visible"] = true,
                    ["rotation"] = 0.0,
                    ["scale"] = new JsonArray(1.0, 1.0),
                    ["origin"] = new JsonArray(0.0, 0.0),
                },
                ["editor"] = new JsonObject(),
                ["children"] = new JsonArray(),
            },
        };
    }

    public static JsonObject CloneForCopy(JsonObject source, string? displayName = null)
    {
        JsonObject copy = (JsonObject)source.DeepClone();
        copy["type"] = UiAssetType;
        if (copy["palette"] is JsonObject palette && !string.IsNullOrWhiteSpace(displayName))
            palette["displayName"] = displayName;
        return copy;
    }

    public static IEnumerable<JsonObject> EnumerateNodes(JsonObject asset)
    {
        if (asset["root"] is not JsonObject root)
            yield break;
        Stack<JsonObject> pending = new Stack<JsonObject>();
        pending.Push(root);
        while (pending.Count != 0)
        {
            JsonObject node = pending.Pop();
            yield return node;
            if (node["children"] is not JsonArray children)
                continue;
            for (int index = children.Count - 1; index >= 0; index--)
            {
                if (children[index] is JsonObject child)
                    pending.Push(child);
            }
        }
    }
}
