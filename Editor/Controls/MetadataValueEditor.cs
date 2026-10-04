using Avalonia.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class MetadataValueEditor : UserControl, IDisposable
{
    private readonly BlueprintVariableForm form;
    private readonly LuaEnumService enums;
    private LuaMetadataType type = LuaMetadataType.Parse("string");
    private JsonNode? value;

    public MetadataValueEditor(string projectPath, LuaEnumService enums)
    {
        this.enums = enums;
        form = new BlueprintVariableForm
        {
            AssetsDirectory = Path.Combine(projectPath, "Assets"),
            ProjectDirectory = projectPath,
            EnumService = enums,
            ShowFieldNames = false,
        };
        form.ValueChanged += (_, args) =>
        {
            value = args.Value?.DeepClone();
            ValueChanged?.Invoke(this, EventArgs.Empty);
        };
        Content = form;
    }

    public event EventHandler? ValueChanged;
    public JsonNode? Value => value?.DeepClone();

    public void SetValue(LuaMetadataType schema, JsonNode? nextValue, JsonObject? meta = null)
    {
        type = schema;
        value = nextValue?.DeepClone();
        JsonObject decorators = meta?.DeepClone() as JsonObject ?? [];
        addFileSelectors(schema, decorators);
        form.SetFields([new BlueprintVariableField("value", schema.ToString(), value)
        {
            Meta = decorators,
            PreserveNullValue = true,
        }]);
    }

    public void Reset(LuaMetadataType schema, JsonObject? meta = null)
    {
        SetValue(schema, schema.Kind == LuaMetadataTypeKind.Named && schema.Name == "file"
            ? JsonValue.Create(string.Empty) : LuaMetadataValueDefaults.Create(schema, _ => null, enums.Read), meta);
    }

    public bool TryValidate(out string? error)
    {
        List<string> errors = [];
        validateEnums(type, errors);
        if (errors.Count == 0)
            LuaMetadataLiteralValidation.ValidateNodeParameter(type, value, "value", errors, enums.Read);
        error = errors.Count == 0 ? null : string.Join(Environment.NewLine, errors);
        return errors.Count == 0;
    }

    public void Dispose()
    {
        form.Dispose();
    }

    private void validateEnums(LuaMetadataType schema, ICollection<string> errors)
    {
        if (schema.Kind == LuaMetadataTypeKind.Enum)
        {
            LuaEnumDefinition definition = enums.Read(schema);
            if (definition.Error is not null)
                errors.Add(definition.Error);
        }
        foreach (LuaMetadataType argument in schema.Arguments)
            validateEnums(argument, errors);
    }

    private static void addFileSelectors(LuaMetadataType schema, JsonObject meta)
    {
        if (schema.Kind == LuaMetadataTypeKind.Named && schema.Name == "file")
        {
            if (!meta.ContainsKey("PathVars"))
                meta["PathVars"] = GameAssetPath.Root;
        }
        else if (schema.Kind is LuaMetadataTypeKind.List or LuaMetadataTypeKind.Dictionary)
        {
            JsonObject item = meta["ItemMeta"] as JsonObject ?? [];
            addFileSelectors(schema.Arguments[schema.Kind == LuaMetadataTypeKind.List ? 0 : 1], item);
            if (item.Count > 0)
                meta["ItemMeta"] = item.DeepClone();
        }
    }
}
