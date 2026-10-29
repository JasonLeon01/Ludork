using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public static class BlueprintMultiSelectionFields
{
    public static IReadOnlyList<BlueprintVariableField> Build(
        IReadOnlyList<IReadOnlyList<BlueprintVariableField>> selections)
    {
        if (selections.Count == 0)
            return [];
        return buildLevel(selections, []);
    }

    internal static IReadOnlyList<BlueprintVariableField> Build(
        IReadOnlyList<IReadOnlyList<BlueprintVariableField>> selections, IReadOnlySet<string> names)
        => selections.Count == 0 ? [] : buildLevel(selections, [], names);

    public static IEnumerable<BlueprintVariableField> EditableLeaves(BlueprintVariableField field)
    {
        if (field.IsReadOnly || field.IsMultiEditReadOnly)
            yield break;
        if (!field.IsMultiEditContainer)
        {
            yield return field;
            yield break;
        }
        foreach (BlueprintVariableField child in field.MultiEditChildren)
            foreach (BlueprintVariableField leaf in EditableLeaves(child))
                yield return leaf;
    }

    public static JsonNode? EffectiveValue(BlueprintVariableField field)
        => field.PreserveNullValue ? field.Value : field.Value ?? field.DefaultValue;

    public static bool TryRead(JsonNode? value, IReadOnlyList<string> path, out JsonNode? result)
    {
        result = value;
        foreach (string segment in path)
        {
            if (result is JsonObject obj && obj.TryGetPropertyValue(segment, out JsonNode? child))
                result = child;
            else if (result is JsonArray array && int.TryParse(segment, out int index)
                && index >= 0 && index < array.Count)
                result = array[index];
            else
                return false;
        }
        return true;
    }

    public static JsonNode? Replace(JsonNode? value, IReadOnlyList<string> path, JsonNode? replacement, bool remove = false)
    {
        if (path.Count == 0)
            return replacement?.DeepClone();
        JsonNode? result = value?.DeepClone();
        JsonNode? parent = result;
        for (int index = 0; index < path.Count - 1; index++)
        {
            if (parent is JsonObject obj)
                parent = obj[path[index]];
            else if (parent is JsonArray array && int.TryParse(path[index], out int itemIndex)
                && itemIndex >= 0 && itemIndex < array.Count)
                parent = array[itemIndex];
            else
                return result;
        }
        string last = path[^1];
        if (parent is JsonObject target)
        {
            if (remove)
                target.Remove(last);
            else
                target[last] = replacement?.DeepClone();
        }
        else if (parent is JsonArray array && int.TryParse(last, out int index)
            && index >= 0 && index < array.Count)
        {
            if (remove)
                array.RemoveAt(index);
            else
                array[index] = replacement?.DeepClone();
        }
        return result;
    }

    public static (JsonNode? Value, bool Remove) ResetCommonValue(BlueprintVariableField current,
        BlueprintVariableField common, JsonNode? defaultValue, bool hasDefault)
    {
        JsonNode? value = EffectiveValue(current);
        if (common.IsReadOnly || common.IsMultiEditReadOnly)
            return (value?.DeepClone(), false);
        if (!hasDefault && current.HasDefaultValue)
        {
            hasDefault = true;
            defaultValue = current.DefaultValue;
        }
        if (!common.IsMultiEditContainer)
            return (defaultValue?.DeepClone(), !hasDefault);
        bool complete = coversAllChildren(current, common);
        if (!hasDefault && complete)
            return (null, true);
        bool union = LuaMetadataType.Parse(current.Type).Kind == LuaMetadataTypeKind.Union;
        if (union && hasDefault && !JsonNode.DeepEquals((value as JsonObject)?["$type"], (defaultValue as JsonObject)?["$type"]))
            return complete ? (defaultValue?.DeepClone(), false) : (value?.DeepClone(), false);
        JsonNode? result = value?.DeepClone();
        IEnumerable<BlueprintVariableField> children = result is JsonArray
            ? common.MultiEditChildren.OrderByDescending(child => int.Parse(child.Name, CultureInfo.InvariantCulture))
            : common.MultiEditChildren;
        foreach (BlueprintVariableField child in children)
        {
            BlueprintVariableField? target = current.MultiEditChildren.FirstOrDefault(field => field.Name == child.Name);
            if (target is null)
                continue;
            bool childHasDefault = TryRead(defaultValue, [child.Name], out JsonNode? childDefault);
            (JsonNode? next, bool remove) = ResetCommonValue(target, child, childDefault, childHasDefault);
            result = Replace(result, [child.Name], next, remove);
        }
        return (result, false);
    }

    private static bool coversAllChildren(BlueprintVariableField current, BlueprintVariableField common)
    {
        if (common.IsReadOnly || common.IsMultiEditReadOnly)
            return false;
        if (!current.IsMultiEditContainer)
            return !common.IsMultiEditContainer;
        if (!common.IsMultiEditContainer || current.MultiEditChildren.Count != common.MultiEditChildren.Count)
            return false;
        foreach (BlueprintVariableField child in current.MultiEditChildren)
        {
            BlueprintVariableField? shared = common.MultiEditChildren.FirstOrDefault(field => field.Name == child.Name);
            if (shared is null || !coversAllChildren(child, shared))
                return false;
        }
        return true;
    }

    private static IReadOnlyList<BlueprintVariableField> buildLevel(
        IReadOnlyList<IReadOnlyList<BlueprintVariableField>> selections, IReadOnlyList<string> parentPath,
        IReadOnlySet<string>? names = null)
    {
        List<BlueprintVariableField> result = [];
        foreach (BlueprintVariableField candidate in selections[0])
        {
            if (names is not null && !names.Contains(candidate.Name))
                continue;
            List<BlueprintVariableField> matches = [];
            foreach (IReadOnlyList<BlueprintVariableField> selection in selections)
            {
                BlueprintVariableField? match = selection.FirstOrDefault(field => field.Name == candidate.Name);
                if (match is null || !compatible(candidate, match))
                    break;
                matches.Add(match);
            }
            if (matches.Count != selections.Count)
                continue;
            BlueprintVariableField field = candidate.Clone();
            field.IsMultiSelection = true;
            field.MultiEditValues = matches.Select(match => EffectiveValue(match)?.DeepClone()).ToArray();
            field.EditPath = parentPath;
            field.IsMixedValue = matches.Skip(1).Any(match => !JsonNode.DeepEquals(EffectiveValue(candidate), EffectiveValue(match)));
            field.IsMultiEditReadOnly = matches.Any(match => match.IsReadOnly);
            for (int index = 0; index < selections.Count; index++)
                field.IsMultiEditReadOnly |= !dependencyMatches(matches[index], selections[index]);
            field.IsMultiEditContainer = matches.Any(isContainer);
            if (field.IsMultiEditContainer && (LuaMetadataType.Parse(field.Type).Kind != LuaMetadataTypeKind.Union
                || matches.Skip(1).All(match => JsonNode.DeepEquals((EffectiveValue(candidate) as JsonObject)?["$type"],
                    (EffectiveValue(match) as JsonObject)?["$type"]))))
            {
                IReadOnlyList<IReadOnlyList<BlueprintVariableField>> children = matches.Select(getChildren).ToArray();
                IReadOnlyList<BlueprintVariableField> commonChildren = buildLevel(children, parentPath);
                foreach (BlueprintVariableField child in commonChildren)
                    prefixPath(child, child.Name);
                field.MultiEditChildren = commonChildren;
            }
            result.Add(field);
        }
        return result;
    }

    private static void prefixPath(BlueprintVariableField field, string segment)
    {
        field.EditPath = new[] { segment }.Concat(field.EditPath).ToArray();
        foreach (BlueprintVariableField child in field.MultiEditChildren)
            prefixPath(child, segment);
    }

    private static bool compatible(BlueprintVariableField left, BlueprintVariableField right)
    {
        return left.Type == right.Type && compatibleValueKinds(left, right) && left.EditorKind == right.EditorKind
            && left.IsComponent == right.IsComponent && left.UseJsonTableEditor == right.UseJsonTableEditor
            && left.RelatedFieldName == right.RelatedFieldName && left.AssetSubdirectory == right.AssetSubdirectory
            && left.RectSourceField == right.RectSourceField && left.Range == right.Range
            && JsonNode.DeepEquals(left.Meta, right.Meta)
            && left.Options.Count == right.Options.Count
            && left.Options.Zip(right.Options).All(pair => pair.First.Label == pair.Second.Label
                && JsonNode.DeepEquals(pair.First.Value, pair.Second.Value))
            && left.Dependency?.Source == right.Dependency?.Source
            && left.Dependency?.Operator == right.Dependency?.Operator
            && JsonNode.DeepEquals(left.Dependency?.ExpectedValue, right.Dependency?.ExpectedValue);
    }

    private static bool compatibleValueKinds(BlueprintVariableField left, BlueprintVariableField right)
    {
        if (!LuaMetadataType.Parse(left.Type).IsAny)
            return true;
        JsonNode? leftValue = EffectiveValue(left);
        JsonNode? rightValue = EffectiveValue(right);
        if (leftValue is null || rightValue is null)
            return true;
        return scalarType(leftValue, string.Empty) == scalarType(rightValue, string.Empty);
    }

    private static bool isContainer(BlueprintVariableField field)
    {
        LuaMetadataType type = LuaMetadataType.Parse(field.Type);
        return EffectiveValue(field) is JsonObject or JsonArray || field.Fields.Count > 0 || field.IsComponent
            || type.Kind is LuaMetadataTypeKind.List or LuaMetadataTypeKind.Dictionary or LuaMetadataTypeKind.Tuple
                or LuaMetadataTypeKind.Table or LuaMetadataTypeKind.Union;
    }

    private static IReadOnlyList<BlueprintVariableField> getChildren(BlueprintVariableField field)
    {
        JsonNode? value = EffectiveValue(field);
        LuaMetadataType type = LuaMetadataType.Parse(field.Type);
        if (type.Kind == LuaMetadataTypeKind.Union)
        {
            if (value is not JsonObject wrapper || !wrapper.ContainsKey("$type") || !wrapper.ContainsKey("$value"))
                return [];
            LuaMetadataType branch = LuaMetadataType.Parse(wrapper["$type"]);
            BlueprintVariableField child = itemField(field, "$value", branch.ToString(), wrapper["$value"], field.Meta);
            return [child];
        }
        if (value is JsonArray array)
        {
            List<BlueprintVariableField> children = [];
            for (int index = 0; index < array.Count; index++)
            {
                string childType = type.Kind == LuaMetadataTypeKind.List ? type.Arguments[0].ToString()
                    : type.Kind == LuaMetadataTypeKind.Tuple && index < type.Arguments.Count ? type.Arguments[index].ToString()
                    : scalarType(array[index], field.Type);
                JsonObject meta = type.Kind == LuaMetadataTypeKind.Tuple && field.Meta["ItemMeta"] is JsonArray tupleMeta
                    && index < tupleMeta.Count ? tupleMeta[index] as JsonObject ?? []
                    : field.Meta["ItemMeta"] as JsonObject ?? [];
                children.Add(itemField(field, index.ToString(CultureInfo.InvariantCulture), childType, array[index], meta));
            }
            return children;
        }
        if (value is not JsonObject obj)
            return [];
        if (field.Fields.Count > 0 && type.Kind == LuaMetadataTypeKind.Named)
        {
            List<BlueprintVariableField> children = [];
            foreach (BlueprintVariableField definition in field.Fields)
            {
                BlueprintVariableField child = definition.Clone();
                if (obj.TryGetPropertyValue(child.Name, out JsonNode? childValue))
                {
                    child.Value = childValue?.DeepClone();
                    child.PreserveNullValue = childValue is null;
                }
                children.Add(child);
            }
            return children;
        }
        string? itemType = type.Kind == LuaMetadataTypeKind.Dictionary ? type.Arguments[1].ToString() : null;
        JsonObject itemMeta = field.Meta["ItemMeta"] as JsonObject ?? [];
        return obj.Select(entry => itemField(field, entry.Key, itemType ?? scalarType(entry.Value, string.Empty), entry.Value, itemMeta)).ToArray();
    }

    private static BlueprintVariableField itemField(BlueprintVariableField parent, string name, string type, JsonNode? value, JsonObject meta)
    {
        LuaTypeReference reference = LuaTypeReference.Parse(type);
        return new BlueprintVariableField(name, type, value)
        {
            Module = reference.ModuleName,
            TypeName = reference.TypeName,
            SourceClass = parent.SourceClass,
            Fields = parent.Fields.Select(field => field.Clone()).ToArray(),
            Meta = meta.DeepClone() as JsonObject ?? [],
            Range = childRange(parent.Type),
            PreserveNullValue = value is null,
            DisplayName = childLabel(parent.Type, name),
        };
    }

    private static BlueprintVariableRange? childRange(string type)
    {
        if (type.EndsWith("Color", StringComparison.OrdinalIgnoreCase) || type.EndsWith("Colour", StringComparison.OrdinalIgnoreCase))
            return new BlueprintVariableRange(0, 255, 1);
        if (type.EndsWith("Vector2u", StringComparison.OrdinalIgnoreCase) || type.EndsWith("Vector3u", StringComparison.OrdinalIgnoreCase))
            return new BlueprintVariableRange(0, uint.MaxValue, 1);
        if (type.EndsWith("Vector2i", StringComparison.OrdinalIgnoreCase) || type.EndsWith("Vector3i", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("IntRect", StringComparison.OrdinalIgnoreCase))
            return new BlueprintVariableRange(int.MinValue, int.MaxValue, 1);
        return null;
    }

    private static string childLabel(string parentType, string name)
    {
        if (!int.TryParse(name, out int index))
            return name;
        if (parentType.EndsWith("Color", StringComparison.OrdinalIgnoreCase)
            || parentType.EndsWith("Colour", StringComparison.OrdinalIgnoreCase))
            return index < 4 ? new[] { "R", "G", "B", "A" }[index] : name;
        if (parentType.EndsWith("IntRect", StringComparison.OrdinalIgnoreCase))
            return index < 4 ? new[] { "X", "Y", "Width", "Height" }[index] : name;
        if (parentType.Contains("Vector", StringComparison.OrdinalIgnoreCase) || parentType == "Pair")
            return index < 3 ? new[] { "X", "Y", "Z" }[index] : name;
        return $"[{index + 1}]";
    }

    private static string scalarType(JsonNode? value, string parentType)
    {
        if (parentType.EndsWith("Vector2f", StringComparison.OrdinalIgnoreCase)
            || parentType.EndsWith("Vector3f", StringComparison.OrdinalIgnoreCase) || parentType == "Pair")
            return "float";
        if (parentType.Contains("Vector", StringComparison.OrdinalIgnoreCase)
            || parentType.EndsWith("IntRect", StringComparison.OrdinalIgnoreCase)
            || parentType.EndsWith("Color", StringComparison.OrdinalIgnoreCase)
            || parentType.EndsWith("Colour", StringComparison.OrdinalIgnoreCase))
            return "int";
        return value is JsonArray ? "any[]" : value is JsonObject ? "Dict[string, any]" : JsonScalar.ScalarType(value) ?? "any";
    }

    private static bool dependencyMatches(BlueprintVariableField field, IReadOnlyList<BlueprintVariableField> context)
    {
        BlueprintVariableDependency? dependency = getDependency(field);
        if (dependency is null)
            return true;
        BlueprintVariableField? sourceField = context.FirstOrDefault(item => item.Name == dependency.Source);
        bool equal = JsonNode.DeepEquals(sourceField is null ? null : EffectiveValue(sourceField), dependency.ExpectedValue);
        return dependency.Operator == "!=" ? !equal : equal;
    }

    internal static string? GetDependencySource(BlueprintVariableField field) => getDependency(field)?.Source;

    private static BlueprintVariableDependency? getDependency(BlueprintVariableField field)
    {
        BlueprintVariableDependency? dependency = field.Dependency;
        JsonNode? rely = field.Meta["Rely"];
        if (dependency is null && rely is JsonObject map && map[field.Name] is JsonNode nested)
            rely = nested;
        if (dependency is null && rely is JsonArray { Count: >= 2 } array
            && JsonScalar.TryGetString(array[0], out string source))
            dependency = new BlueprintVariableDependency(source, array[1]);
        if (dependency is null && rely is JsonObject rule)
        {
            string? sourceName = readString(rule["source"]) ?? readString(rule["key"]) ?? readString(rule["var"]);
            if (sourceName is not null)
                dependency = new BlueprintVariableDependency(sourceName, rule["value"], readString(rule["op"]) ?? readString(rule["operator"]) ?? "==");
        }
        return dependency;
    }

    private static string? readString(JsonNode? value)
        => JsonScalar.TryGetString(value, out string text) ? text : null;
}
