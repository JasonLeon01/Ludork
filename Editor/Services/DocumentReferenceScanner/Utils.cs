using Ludork.Models;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
{
    private string generalMemberNodeId(string typeKey, string memberKey)
    {
        string id = ReferenceIdentity.GeneralMemberNodeId(typeKey, memberKey);
        generalMemberTypes[id] = typeKey;
        return id;
    }

    private static string? blueprintNodeIdFromClassPath(JsonNode? value)
    {
        string? text = JsonScalar.String(value)?.Trim();
        return BlueprintReference.IsReference(text)
            ? ReferenceIdentity.NodeId("blueprint", BlueprintReference.ToReference(text))
            : null;
    }

    private static JsonNode? parameterAt(JsonArray parameters, int index)
    {
        return index >= 0 && index < parameters.Count ? parameters[index] : null;
    }

    private static string? getMetaReference(JsonNode? value, string name)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out string? text))
                return text;
            if (scalar.TryGetValue(out bool enabled) && enabled)
                return string.Empty;
            return null;
        }
        if (value is JsonObject objectValue)
        {
            if (objectValue.TryGetPropertyValue(name, out JsonNode? named))
                return JsonScalar.String(named) ?? string.Empty;
            return null;
        }
        if (value is not JsonArray array)
            return null;
        foreach (JsonNode? item in array)
        {
            if (string.Equals(JsonScalar.String(item), name, StringComparison.Ordinal))
                return string.Empty;
            if (item is JsonArray tuple
                && tuple.Count != 0
                && string.Equals(JsonScalar.String(tuple[0]), name, StringComparison.Ordinal))
            {
                return tuple.Count > 1 ? JsonScalar.String(tuple[1]) ?? string.Empty : string.Empty;
            }
        }
        return null;
    }

    private static string normalizeAssetPath(JsonNode? value)
    {
        string? text = JsonScalar.String(value);
        return GameAssetPath.IsCanonical(text) ? text! : string.Empty;
    }

    private static string normalizeExplicitAssetPath(string value)
    {
        return GameAssetPath.IsCanonical(value) ? value : string.Empty;
    }

    private static string normalizeDataReference(string value, string section)
    {
        string normalized = value.Replace('\\', '/').Trim().Trim('/');
        if (section.Equals("Blueprints", StringComparison.OrdinalIgnoreCase))
        {
            const string blueprintRootPrefix = "Blueprints/";
            return BlueprintReference.NormalizeKey(normalized.StartsWith(blueprintRootPrefix, StringComparison.OrdinalIgnoreCase)
                ? normalized[blueprintRootPrefix.Length..]
                : normalized);
        }
        string dottedPrefix = "Data." + section + ".";
        if (normalized.StartsWith(dottedPrefix, StringComparison.Ordinal))
            normalized = normalized[dottedPrefix.Length..].Replace('.', '/');
        string slashPrefix = section + "/";
        if (normalized.StartsWith(slashPrefix, StringComparison.OrdinalIgnoreCase))
            normalized = normalized[slashPrefix.Length..];
        if (normalized.EndsWith(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            normalized = normalized[..^DataConfig.DataFileExtension.Length];
        return normalized;
    }

}
