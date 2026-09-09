using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace Ludork.Services;

public sealed class UiControlRegistryService : IDisposable
{
    private readonly GameDataService gameData;

    public UiControlRegistryService(GameDataService gameData)
    {
        this.gameData = gameData;
        Runtime = new UiPreviewRuntimeService(gameData.ProjectPath);
    }

    public UiPreviewRuntimeService Runtime { get; }
    public bool IsReady => Runtime.IsReady;
    public IReadOnlyList<UiControlDescriptor> SystemDescriptors => Runtime.Current?.Controls ?? [];
    public string? ExpectedTextConfigType(string controlId) => SystemDescriptors
        .FirstOrDefault(control => control.ControlId == controlId)?.TextKind switch
        {
            "plain" => "plainTextConfig",
            "rich" => "richTextConfig",
            _ => null,
        };

    public void Dispose() => Runtime.Dispose();

    internal static bool IsCanonicalControlId(string controlId)
        => Regex.IsMatch(controlId, @"\A[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)+\z");

    public IReadOnlyList<UiControlDescriptor> GetDescriptors(
        bool includeUnexposedProjectAssets = false)
    {
        List<UiControlDescriptor> descriptors = new List<UiControlDescriptor>(SystemDescriptors);
        descriptors.AddRange(createProjectDescriptors(!includeUnexposedProjectAssets));
        return descriptors
            .OrderBy(descriptor => descriptor.Category, CodePointComparer.Instance)
            .ThenBy(descriptor => descriptor.DisplayName, CodePointComparer.Instance)
            .ThenBy(descriptor => descriptor.ControlId, CodePointComparer.Instance)
            .ToArray();
    }

    public IReadOnlyDictionary<string, UiControlDescriptor> CreateControlLookup(
        bool includeUnexposedProjectAssets = false)
    {
        return GetDescriptors(includeUnexposedProjectAssets)
            .GroupBy(descriptor => descriptor.ControlId, StringComparer.Ordinal)
            .ToDictionary(
                group => group.Key,
                group => group.First(),
                StringComparer.Ordinal);
    }

    private IReadOnlyList<UiControlDescriptor> createProjectDescriptors(bool exposedOnly)
    {
        List<UiControlDescriptor> descriptors = [];
        foreach (KeyValuePair<string, JsonObject> pair in gameData.UiAssetsData
                     .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            JsonObject asset = pair.Value;
            bool exposed = isTrue((asset["palette"] as JsonObject)?["exposed"]);
            string logicalKey = UiAssetSchema.ToLogicalAssetKey(pair.Key);
            if (logicalKey.Length == 0 || exposedOnly && !exposed)
            {
                continue;
            }
            JsonObject? palette = asset["palette"] as JsonObject;
            string displayName = getString(palette?["displayName"])?.Trim()
                ?? pair.Key.Split('/').Last();
            string category = getString(palette?["category"])?.Trim() ?? "Project";
            UiDesignSize designSize = readDesignSize(asset["designSize"] as JsonObject);
            descriptors.Add(new UiControlDescriptor(
                UiAssetSchema.ProjectControlPrefix + logicalKey,
                "project",
                displayName.Length == 0 ? pair.Key.Split('/').Last() : displayName,
                category.Length == 0 ? "Project" : category,
                null,
                "none",
                null,
                [],
                logicalKey,
                designSize));
        }
        return descriptors;
    }

    private static UiDesignSize readDesignSize(JsonObject? designSize)
    {
        double width = getFiniteNumber(designSize?["width"]) ?? 640.0;
        double height = getFiniteNumber(designSize?["height"]) ?? 480.0;
        return new UiDesignSize(
            width,
            height,
            designSize?["width"]?.ToJsonString(),
            designSize?["height"]?.ToJsonString());
    }

    internal static string CreateAdapterFingerprint(IReadOnlyList<UiControlDescriptor> descriptors)
    {
        StringBuilder source = new StringBuilder();
        foreach (UiControlDescriptor descriptor in descriptors
                     .OrderBy(item => item.ControlId, CodePointComparer.Instance))
        {
            source.Append(descriptor.ControlId);
            source.Append('|');
            source.Append(descriptor.Adapter ?? string.Empty);
            source.Append('|');
            source.Append(descriptor.ChildPolicy);
            source.Append('|');
            source.Append(descriptor.SlotType ?? string.Empty);
            source.Append('|');
            foreach (UiControlPropertyDescriptor propertyDescriptor in
                     descriptor.Properties.Where(property => !property.EditorOnly))
            {
                source.Append(propertyDescriptor.Id);
                source.Append(':');
                source.Append(propertyDescriptor.Type);
                source.Append(':');
                source.Append(propertyDescriptor.Required ? "true" : "false");
                source.Append(';');
            }
            source.Append('\n');
        }
        byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(source.ToString()));
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static double? getFiniteNumber(JsonNode? value)
    {
        if (value is not JsonValue scalar || !scalar.TryGetValue(out double number) || !double.IsFinite(number))
            return null;
        return number;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static bool isTrue(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out bool enabled) && enabled;
    }

    private sealed class CodePointComparer : IComparer<string>
    {
        public static CodePointComparer Instance { get; } = new CodePointComparer();

        public int Compare(string? left, string? right)
        {
            if (ReferenceEquals(left, right))
                return 0;
            if (left is null)
                return -1;
            if (right is null)
                return 1;
            StringRuneEnumerator leftRunes = left.EnumerateRunes();
            StringRuneEnumerator rightRunes = right.EnumerateRunes();
            while (leftRunes.MoveNext())
            {
                if (!rightRunes.MoveNext())
                    return 1;
                int comparison = leftRunes.Current.Value.CompareTo(rightRunes.Current.Value);
                if (comparison != 0)
                    return comparison;
            }
            return rightRunes.MoveNext() ? -1 : 0;
        }
    }
}
