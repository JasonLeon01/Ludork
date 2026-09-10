using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class JsonDataEdit
{
    public enum Operation { Set, Remove, Insert }

    private readonly JsonNode? value;

    public JsonDataEdit(Operation operation, IEnumerable<object> path, JsonNode? value = null)
    {
        object[] segments = path.ToArray();
        if (segments.Length == 0 || segments.Any(segment => segment is not string && segment is not int))
            throw new ArgumentException("An edit requires property names or array indices.", nameof(path));
        Kind = operation;
        Path = Array.AsReadOnly(segments);
        this.value = value?.DeepClone();
    }

    public Operation Kind { get; }
    public IReadOnlyList<object> Path { get; }

    public bool CanApply(JsonObject target)
    {
        JsonNode? current = target;
        for (int index = 0; index < Path.Count; index++)
        {
            object segment = Path[index];
            if (current is not null && (segment is string && current is not JsonObject
                    || segment is int && current is not JsonArray)
                || segment is int number && number < 0)
                return false;
            if (index == Path.Count - 1 && Kind == Operation.Insert)
                return segment is int insertion && insertion <= ((current as JsonArray)?.Count ?? 0);
            current = current is null ? null : readChild(current, segment);
        }
        return true;
    }

    public bool Changes(JsonObject target)
    {
        JsonNode? current = target;
        bool exists = true;
        foreach (object segment in Path)
        {
            if (segment is string name && current is JsonObject obj)
                exists = obj.TryGetPropertyValue(name, out current);
            else if (segment is int index && current is JsonArray array && index >= 0 && index < array.Count)
                current = array[index];
            else
                exists = false;
            if (!exists)
                break;
        }
        return Kind == Operation.Insert || (Kind == Operation.Remove
            ? exists
            : !exists || !JsonNode.DeepEquals(current, value));
    }

    public void Apply(JsonObject target)
    {
        JsonNode current = target;
        for (int index = 0; index < Path.Count - 1; index++)
        {
            object segment = Path[index];
            JsonNode? child = readChild(current, segment);
            if (child is null)
            {
                if (Kind == Operation.Remove)
                    return;
                child = Path[index + 1] is int ? new JsonArray() : new JsonObject();
                setChild(current, segment, child);
            }
            current = child;
        }
        object last = Path[^1];
        if (Kind == Operation.Remove)
        {
            if (current is JsonObject obj && last is string name)
                obj.Remove(name);
            else if (current is JsonArray array && last is int index && index >= 0 && index < array.Count)
                array.RemoveAt(index);
        }
        else if (Kind == Operation.Insert && current is JsonArray array && last is int index)
        {
            array.Insert(index, value?.DeepClone());
        }
        else
        {
            setChild(current, last, value?.DeepClone());
        }
    }

    private static JsonNode? readChild(JsonNode parent, object segment)
    {
        return segment switch
        {
            string name when parent is JsonObject obj => obj[name],
            int index when parent is JsonArray array && index >= 0 && index < array.Count => array[index],
            _ => null,
        };
    }

    private static void setChild(JsonNode parent, object segment, JsonNode? child)
    {
        if (parent is JsonObject obj && segment is string name)
        {
            obj[name] = child;
            return;
        }
        if (parent is JsonArray array && segment is int index && index >= 0)
        {
            while (array.Count <= index)
                array.Add(null);
            array[index] = child;
            return;
        }
        throw new InvalidOperationException("The edit target no longer has the expected shape.");
    }
}
