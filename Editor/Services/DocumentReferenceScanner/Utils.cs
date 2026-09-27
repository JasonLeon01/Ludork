using Ludork.Models;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
{
    private static string nodeId(string type, string key)
    {
        return $"{type}:{key.Replace('\\', '/')}";
    }

    private string generalMemberNodeId(string typeKey, string memberKey)
    {
        string id = nodeId("generalMember", $"{typeKey}/{memberKey}");
        generalMemberTypes[id] = typeKey;
        return id;
    }

    private static string blueprintNodeIdFromKey(string key)
    {
        return nodeId("blueprint", BlueprintPrefix + key.Replace('/', '.'));
    }

    private static string? blueprintNodeIdFromClassPath(JsonNode? value)
    {
        string? text = getString(value)?.Trim();
        return text is not null && text.StartsWith(BlueprintPrefix, StringComparison.Ordinal)
            ? nodeId("blueprint", text)
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
                return getString(named) ?? string.Empty;
            return null;
        }
        if (value is not JsonArray array)
            return null;
        foreach (JsonNode? item in array)
        {
            if (string.Equals(getString(item), name, StringComparison.Ordinal))
                return string.Empty;
            if (item is JsonArray tuple
                && tuple.Count != 0
                && string.Equals(getString(tuple[0]), name, StringComparison.Ordinal))
            {
                return tuple.Count > 1 ? getString(tuple[1]) ?? string.Empty : string.Empty;
            }
        }
        return null;
    }

    private static string normalizeAssetPath(JsonNode? value)
    {
        string? text = getString(value);
        return GameAssetPath.IsCanonical(text) ? text! : string.Empty;
    }

    private static string normalizeExplicitAssetPath(string value)
    {
        return GameAssetPath.IsCanonical(value) ? value : string.Empty;
    }

    private static string? normalizeReferenceParam(JsonNode? value)
    {
        string? text = getString(value)?.Trim();
        if (string.IsNullOrWhiteSpace(text))
            return null;
        if (text.Length >= 2
            && text[0] == text[^1]
            && text[0] is '\'' or '"')
        {
            text = text[1..^1].Trim();
        }
        return text.Length == 0 ? null : text;
    }

    private static string normalizeDataReference(string value, string section)
    {
        string normalized = value.Replace('\\', '/').Trim().Trim('/');
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

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

}
