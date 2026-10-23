using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
{
    private static void collectStoredValues(JsonNode node, string path, string pointer,
        IDictionary<(string Path, bool IsDictionaryKey), string> values)
    {
        if (node is JsonObject data)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in data)
            {
                string childPath = path + "." + pair.Key;
                addStoredValue(values, (childPath, true), pointer + "/" + escapePointer(pair.Key));
                if (pair.Value is not null)
                    collectStoredValues(pair.Value, childPath, pointer + "/" + escapePointer(pair.Key), values);
            }
        }
        else if (node is JsonArray array)
        {
            for (int index = 0; index < array.Count; index++)
                if (array[index] is JsonNode child)
                    collectStoredValues(child, path + "[" + index + "]", pointer + "/" + index, values);
        }
        else if (JsonScalar.String(node) is not null)
            addStoredValue(values, (path, false), pointer);
    }

    private static string escapePointer(string value) => value.Replace("~", "~0").Replace("/", "~1");

    private static void addStoredValue(IDictionary<(string Path, bool IsDictionaryKey), string> values,
        (string Path, bool IsDictionaryKey) key, string pointer)
    {
        values[key] = values.ContainsKey(key) ? string.Empty : pointer;
    }

    private static string? getStoragePath(ReferenceRecord reference, string section, string key, JsonObject data,
        IReadOnlyDictionary<(string Path, bool IsDictionaryKey), string> storedValues)
    {
        if (reference.StoragePath is null)
            return null;
        string path = reference.Path;
        string root = section + "/" + key;
        if (section == "General" && data["members"] is JsonObject members)
        {
            foreach (KeyValuePair<string, JsonNode?> member in members)
            {
                if (reference.Source != ReferenceIdentity.GeneralMemberNodeId(key, member.Key))
                    continue;
                string prefix = root + "/" + member.Key + ".";
                if (path.StartsWith(prefix, StringComparison.Ordinal))
                    path = root + ".members." + member.Key + "." + path[prefix.Length..];
                break;
            }
        }
        if (section == "WorldMaps" && path.StartsWith("Maps/" + key + "/_world.", StringComparison.Ordinal))
            path = root + path[("Maps/" + key + "/_world").Length..];
        if (!path.StartsWith(root + ".", StringComparison.Ordinal))
            path = root + "." + path;
        return storedValues.GetValueOrDefault((path, reference.IsDictionaryKey));
    }
}
