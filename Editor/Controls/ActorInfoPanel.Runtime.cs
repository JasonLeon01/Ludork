using Avalonia.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class ActorInfoPanel
{
    public void ConfigureEditingContext(IMapEditingContext context)
    {
        if (ReferenceEquals(editingContext, context))
            return;
        editingContext = context;
        runtimeActorId = null;
        displayedRuntimeValues = null;
        displayedRuntimeSchema = null;
        classForm.HistoryGameData = context.IsRuntime ? null : gameData;
        if (context.IsRuntime)
        {
            HistoryMergeBehavior.Detach(tagEdit);
            HistoryMergeBehavior.Detach(positionX);
            HistoryMergeBehavior.Detach(positionY);
            HistoryMergeBehavior.DetachBoundary(this);
        }
        else if (gameData is not null)
        {
            HistoryMergeBehavior.Attach(tagEdit, gameData);
            HistoryMergeBehavior.Attach(positionX, gameData);
            HistoryMergeBehavior.Attach(positionY, gameData);
            HistoryMergeBehavior.AttachBoundary(this, gameData);
        }
        updateEditableState();
        refreshActorInfo();
    }

    private void refreshRuntimeActorInfo()
    {
        JsonObject? actor = getActorData();
        if (actor is null)
        {
            setActor(mapKey ?? string.Empty, null, null, null);
            return;
        }
        if (!positionX.IsKeyboardFocusWithin && !positionY.IsKeyboardFocusWithin)
        {
            loading = true;
            updatePositionEditors(actor);
            loading = false;
        }
        updateEditableState();
        if (!classForm.IsKeyboardFocusWithin)
            refreshRuntimeClassDetail();
    }

    private void refreshRuntimeClassDetail()
    {
        if (runtimeActorId is null || fieldBuilder is null
            || getMapData()?["runtimeInfo"]?[runtimeActorId] is not JsonObject info
            || info["schema"] is not JsonObject schema)
        {
            clearClassDetail();
            displayedRuntimeValues = null;
            displayedRuntimeSchema = null;
            return;
        }
        JsonObject values = getMapData()?["BPClassVarChanged"]?[runtimeActorId] as JsonObject
            ?? info["values"] as JsonObject ?? [];
        if (JsonNode.DeepEquals(displayedRuntimeValues, values) && JsonNode.DeepEquals(displayedRuntimeSchema, schema))
            return;
        string reference = getActorData()?["type"]?.GetValue<string>() ?? blueprintReference ?? "Engine.Actor";
        LuaTypeReference declaringType = LuaTypeReference.Parse(reference);
        List<ResolvedBlueprintField> fields = [];
        Dictionary<string, BlueprintVariableField> readOnlyFields = new(StringComparer.Ordinal);
        foreach (KeyValuePair<string, JsonNode?> entry in schema)
        {
            if (entry.Value is not JsonObject descriptor || isRuntimeIdentityField(entry.Key)
                || !values.TryGetPropertyValue(entry.Key, out JsonNode? value))
                continue;
            JsonObject meta = descriptor["Meta"] as JsonObject ?? [];
            bool component = descriptor["component"]?.GetValue<bool>() ?? false;
            if (isBlueprintOnly(meta["BlueprintOnly"]) || component)
                continue;
            if (descriptor["readOnly"]?.GetValue<bool>() == true)
            {
                string typeName = descriptor["type"] is JsonValue typeValue && typeValue.TryGetValue(out string? name)
                    ? name : descriptor["type"]?.ToJsonString() ?? "any";
                string summary = value is JsonValue scalar && scalar.TryGetValue(out string? text)
                    ? text : value?.ToJsonString() ?? "null";
                readOnlyFields[entry.Key] = new BlueprintVariableField(entry.Key, "string", value)
                {
                    TypeName = typeName,
                    Description = typeName,
                    DisplayValue = JsonValue.Create(typeName + ": " + summary),
                    SourceClass = reference,
                    IsReadOnly = true,
                    PreserveNullValue = true,
                };
                continue;
            }
            LuaTypeReference fieldType = LuaTypeReference.FromSchema(LuaMetadataType.Parse(descriptor["type"]));
            BlueprintFieldMetadata metadata = new(entry.Key, fieldType, true, value, component, meta, declaringType);
            fields.Add(new ResolvedBlueprintField(entry.Key, fieldType, value, value, metadata, false, true, reference));
        }
        ResolvedBlueprintClass resolved = new(reference, reference, declaringType, fields, [], [], [],
            false, false, false, [], [], null, 0, 0);
        defaultValues.Clear();
        displayValues.Clear();
        fieldsWithDefaults.Clear();
        overriddenFields.Clear();
        resetButtons.Clear();
        Dictionary<string, BlueprintVariableField> availableFields = new(readOnlyFields, StringComparer.Ordinal);
        foreach (BlueprintVariableField field in fieldBuilder.Build(resolved))
            availableFields[field.Name] = field;
        List<BlueprintVariableField> formFields = [];
        foreach (KeyValuePair<string, JsonNode?> entry in schema)
            if (availableFields.TryGetValue(entry.Key, out BlueprintVariableField? field))
                formFields.Add(field);
        classForm.SetFields(formFields);
        displayedRuntimeValues = values.DeepClone() as JsonObject;
        displayedRuntimeSchema = schema.DeepClone() as JsonObject;
        updateResetActions();
        setClassDetailVisible(formFields.Count != 0);
    }

    private void setRuntimeActorVariable(BlueprintVariableValueChangedEventArgs args)
    {
        if (loading || !canEditActor || editingContext is null || mapKey is null || layerName is null
            || runtimeActorId is null || isRuntimeIdentityField(args.Name)
            || getMapData()?["runtimeInfo"]?[runtimeActorId]?["schema"]?[args.Name] is not JsonObject descriptor
            || descriptor["readOnly"]?.GetValue<bool>() == true
            || descriptor["component"]?.GetValue<bool>() == true)
            return;
        JsonObject? values = getClassVarChanges(getEditableActorData() ?? []);
        if (values is null || !values.ContainsKey(args.Name)
            || (descriptor["component"]?.GetValue<bool>() ?? false) && (values[args.Name] is null || args.Value is null))
            return;
        displayedRuntimeValues = null;
        if (!editingContext.SetActorVariable(mapKey, layerName, runtimeActorId, args.Name, cloneNode(args.Value)))
            refreshRuntimeClassDetail();
    }

    private static bool isRuntimeIdentityField(string name)
        => name is "tag" or "runtimeId" or "bp" or "type" or "parent" or "parentClass" or "scriptMixin" or "scriptPath";
}
