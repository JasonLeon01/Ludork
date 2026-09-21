using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal static string normalizeDataKey(string key)
    {
        return key.Replace('\\', '/').Trim().Trim('/');
    }

    internal static string normalizeJsonKey(string key)
    {
        string normalized = normalizeDataKey(key);
        return normalized.EndsWith(".json", StringComparison.OrdinalIgnoreCase)
            ? normalized[..^5]
            : normalized;
    }

    internal bool renameDataEntry(string sectionName, string oldKey, string newKey)
    {
        return RenameDocumentResource(sectionName, oldKey, newKey);
    }

    internal bool deleteDataEntry(string sectionName, string key)
    {
        return DeleteDocumentResource(sectionName, key);
    }

    internal static JsonObject cloneObject(JsonNode? value)
    {
        return value is JsonObject objectValue ? (JsonObject)objectValue.DeepClone() : new JsonObject();
    }

    internal Dictionary<string, Dictionary<string, JsonObject>> cloneAllData()
    {
        return sections.ToDictionary(
            section => section.Key,
            section => section.Value.ToDictionary(item => item.Key,
                item => (JsonObject)item.Value.DeepClone(), StringComparer.Ordinal),
            StringComparer.Ordinal);
    }

    internal static bool hasDataFileExtension(string sectionName, string path)
    {
        StringComparison comparison = sectionName == "UI"
            ? StringComparison.Ordinal
            : StringComparison.OrdinalIgnoreCase;
        return string.Equals(
            Path.GetExtension(path),
            DataConfig.DataFileExtension,
            comparison);
    }

    internal static string formatSaveDetails(
        IReadOnlyList<string> added,
        IReadOnlyList<string> updated,
        IReadOnlyList<string> deleted,
        IReadOnlyList<string> failed
    )
    {
        List<string> lines = [];
        if (added.Count != 0)
            lines.Add($"A [{string.Join(", ", added)}]");
        if (updated.Count != 0)
            lines.Add($"U [{string.Join(", ", updated)}]");
        if (deleted.Count != 0)
            lines.Add($"D [{string.Join(", ", deleted)}]");
        if (failed.Count != 0)
            lines.Add($"Failed [{string.Join(", ", failed)}]");
        return "\n" + string.Join("\n", lines);
    }

    internal static bool nodesEqual(JsonNode current, JsonNode origin)
    {
        return string.Equals(current.ToJsonString(), origin.ToJsonString(), StringComparison.Ordinal);
    }

    internal static void deleteTemporaryFile(string path)
    {
        try
        {
            if (File.Exists(path))
                File.Delete(path);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
        }
    }

    internal static bool isUiDataType(JsonObject data, string expectedType)
    {
        return string.Equals(getString(data["type"]), expectedType, StringComparison.Ordinal);
    }

    internal static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

}
