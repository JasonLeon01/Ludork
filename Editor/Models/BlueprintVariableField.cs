using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintVariableField
{
    public BlueprintVariableField(string name, string type, JsonNode? value = null)
    {
        Name = name;
        Type = type;
        Value = value?.DeepClone();
    }

    public string Name { get; init; }
    public string? DisplayName { get; init; }
    public string Description { get; init; } = string.Empty;
    public string Type { get; init; }
    public string? Module { get; init; }
    public string? TypeName { get; init; }
    public string? SourceClass { get; init; }
    public JsonNode? Value { get; set; }
    public JsonNode? DefaultValue { get; init; }
    public bool HasDefaultValue { get; init; }
    public JsonNode? DisplayValue { get; set; }
    public JsonObject Meta { get; init; } = [];
    public bool IsMixedValue { get; set; }
    public bool IsMultiSelection { get; set; }
    public bool IsMultiEditReadOnly { get; set; }
    public bool IsMultiEditContainer { get; set; }
    public IReadOnlyList<string> EditPath { get; set; } = [];
    public IReadOnlyList<JsonNode?> MultiEditValues { get; set; } = [];
    public IReadOnlyList<BlueprintVariableField> MultiEditChildren { get; set; } = [];
    public bool IsComponent { get; init; }
    public bool IsReadOnly { get; init; }
    public bool UseJsonTableEditor { get; init; }
    public bool PreserveNullValue { get; set; }
    public BlueprintVariableEditorKind EditorKind { get; init; }
    public string? RelatedFieldName { get; init; }
    public string? AssetSubdirectory { get; init; }
    public string? RectSourceField { get; init; }
    public BlueprintVariableDependency? Dependency { get; init; }
    public BlueprintVariableRange? Range { get; init; }
    public IReadOnlyList<BlueprintVariableOption> Options { get; init; } = [];
    public IReadOnlyList<BlueprintVariableField> Fields { get; init; } = [];

    public BlueprintVariableField Clone()
    {
        return Clone(Name);
    }

    public BlueprintVariableField Clone(string name)
    {
        return new BlueprintVariableField(name, Type, Value)
        {
            DisplayName = DisplayName,
            Description = Description,
            Module = Module,
            TypeName = TypeName,
            SourceClass = SourceClass,
            DefaultValue = DefaultValue?.DeepClone(),
            HasDefaultValue = HasDefaultValue,
            DisplayValue = DisplayValue?.DeepClone(),
            Meta = Meta.DeepClone() as JsonObject ?? [],
            IsMixedValue = IsMixedValue,
            IsMultiSelection = IsMultiSelection,
            IsMultiEditReadOnly = IsMultiEditReadOnly,
            IsMultiEditContainer = IsMultiEditContainer,
            EditPath = EditPath.ToArray(),
            MultiEditValues = MultiEditValues.Select(value => value?.DeepClone()).ToArray(),
            MultiEditChildren = MultiEditChildren.Select(field => field.Clone()).ToArray(),
            IsComponent = IsComponent,
            IsReadOnly = IsReadOnly,
            UseJsonTableEditor = UseJsonTableEditor,
            PreserveNullValue = PreserveNullValue,
            EditorKind = EditorKind,
            RelatedFieldName = RelatedFieldName,
            AssetSubdirectory = AssetSubdirectory,
            RectSourceField = RectSourceField,
            Dependency = Dependency?.Clone(),
            Range = Range,
            Options = Options.Select(option => option.Clone()).ToArray(),
            Fields = Fields.Select(field => field.Clone()).ToArray(),
        };
    }
}
