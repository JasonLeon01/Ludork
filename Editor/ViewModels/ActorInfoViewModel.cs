using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public sealed class ActorInfoViewModel
{
    private readonly Dictionary<string, JsonNode?> defaultValues = new(StringComparer.Ordinal);
    private readonly HashSet<string> fieldsWithDefaults = new(StringComparer.Ordinal);
    private readonly HashSet<string> overriddenFields = new(StringComparer.Ordinal);
    private ProjectDataStore? gameData;
    private IMapEditingContext? editingContext;
    private MapDocumentSnapshot? mapSnapshot;
    private string? runtimeActorId;
    private JsonObject? displayedRuntimeValues;
    private JsonObject? displayedRuntimeSchema;
    private bool isRuntime => editingContext?.IsRuntime == true;
    private bool canEditActor => layerEditable && editingContext?.IsEditable == true;
    private bool canMoveActor => canEditActor && (!isRuntime || getActorData()?.ParentRuntimeId is null);
    private LuaMetadataService? metadataService;
    private BlueprintClassResolver? classResolver;
    private BlueprintVariableFieldBuilder? fieldBuilder;
    private string? mapKey;
    private string? layerName;
    private int? actorIndex;
    private string? actorTag;
    private bool layerEditable = true;
    private string? blueprintReference;

    public bool IsRuntime => isRuntime;
    public bool HasSelection { get; private set; }
    public bool CanEdit => canEditActor && HasSelection;
    public bool CanMove => canMoveActor && HasSelection;
    public bool HasProjectBlueprint => tryGetProjectBlueprintReference(out _);
    public bool HasOverrides => overriddenFields.Count != 0;
    public bool ClassDetailVisible { get; private set; }
    public string Tag { get; private set; } = string.Empty;
    public string BlueprintReference { get; private set; } = string.Empty;
    public int PositionX { get; private set; }
    public int PositionY { get; private set; }
    public int MaximumX { get; private set; }
    public int MaximumY { get; private set; }
    public string MapKey => mapKey ?? string.Empty;
    public string? LayerName => layerName;
    public int? ActorIndex => actorIndex;
    public MapActorSnapshot? ActorData => getActorData();
    public IReadOnlyList<BlueprintVariableField> FormFields { get; private set; } = [];
    public bool IsOverridden(string name) => overriddenFields.Contains(name);
    public event EventHandler? HeaderChanged;
    public event EventHandler? FieldsChanged;
    public event EventHandler? EditableStateChanged;
    public event EventHandler? OverridesChanged;
    public event EventHandler? RefreshSelectedActorRequested;
    public event EventHandler? ActorTagChanged;
    public event EventHandler? ClassRefreshRequested;

    public void Configure(ProjectDataStore data, LuaMetadataService metadata, BlueprintClassResolver resolver)
    {
        gameData = data;
        metadataService = metadata;
        classResolver = resolver;
        fieldBuilder = new BlueprintVariableFieldBuilder(data, metadata);
        ConfigureEditingContext(new ProjectMapEditingContext(data));
    }

    public bool ConfigureEditingContext(IMapEditingContext context)
    {
        if (ReferenceEquals(editingContext, context))
            return false;
        editingContext = context;
        runtimeActorId = null;
        displayedRuntimeValues = null;
        displayedRuntimeSchema = null;
        EditableStateChanged?.Invoke(this, EventArgs.Empty);
        refreshActorInfo();
        return true;
    }

    private void SetSelection(bool selected)
    {
        HasSelection = selected;
        HeaderChanged?.Invoke(this, EventArgs.Empty);
    }

    private void SetClassDetailVisible(bool visible)
    {
        ClassDetailVisible = visible;
        EditableStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private MapDocumentSnapshot? getMapData() => mapSnapshot;

    private void readMapSnapshot()
    {
        mapSnapshot = mapKey is null ? null : editingContext?.ReadMapDocument(mapKey);
    }

    private void refreshRuntimeActorInfo(bool preservePositionDraft = false, bool preserveClassDraft = false)
    {
        MapActorSnapshot? actor = getActorData();
        if (actor is null)
        {
            setActor(mapKey ?? string.Empty, null, null, null);
            return;
        }
        if (!preservePositionDraft)
        {
            updatePositionEditors(actor);
            HeaderChanged?.Invoke(this, EventArgs.Empty);
        }
        EditableStateChanged?.Invoke(this, EventArgs.Empty);
        if (!preserveClassDraft)
            refreshRuntimeClassDetail();
    }

    private static bool isRuntimeIdentityField(string name)
        => name is "tag" or "runtimeId" or "bp" or "type" or "parent" or "parentClass" or "scriptMixin" or "scriptPath";

    public void setActor(
        string nextMapKey,
        string? nextLayerName,
        int? nextActorIndex,
        MapActorSnapshot? actorData, bool preservePositionDraft = false, bool preserveClassDraft = false)
    {
        bool sameRuntimeActor = isRuntime && runtimeActorId is not null
            && string.Equals(runtimeActorId, actorData?.RuntimeId, StringComparison.Ordinal)
            && string.Equals(mapKey, nextMapKey, StringComparison.Ordinal);
        mapKey = string.IsNullOrWhiteSpace(nextMapKey) ? null : nextMapKey;
        readMapSnapshot();
        layerName = nextLayerName;
        actorIndex = nextActorIndex;
        runtimeActorId = isRuntime ? actorData?.RuntimeId : null;
        if (sameRuntimeActor)
        {
            refreshRuntimeActorInfo(preservePositionDraft, preserveClassDraft);
            return;
        }
        displayedRuntimeValues = null;
        displayedRuntimeSchema = null;
        if (mapKey is null || layerName is null || actorIndex is null || actorData is null)
        {
            mapKey = null;
            layerName = null;
            actorIndex = null;
            actorTag = null;
            blueprintReference = null;
            runtimeActorId = null;
            mapSnapshot = null;
            Tag = string.Empty;
            BlueprintReference = string.Empty;
            SetSelection(false);
            clearClassDetail();
            return;
        }

        SetSelection(true);
        actorTag = actorData.Tag;
        Tag = actorTag;
        blueprintReference = actorData.Blueprint;
        if (isRuntime && string.IsNullOrWhiteSpace(blueprintReference))
            blueprintReference = actorData.Type;
        BlueprintReference = blueprintReference ?? string.Empty;
        updatePositionEditors(actorData);
        EditableStateChanged?.Invoke(this, EventArgs.Empty);
        refreshClassDetail();
        HeaderChanged?.Invoke(this, EventArgs.Empty);
    }

    public void setLayerEditable(bool editable)
    {
        if (layerEditable == editable)
            return;
        layerEditable = editable;
        EditableStateChanged?.Invoke(this, EventArgs.Empty);
    }

    public void refreshActorPosition(bool preservePositionDraft = false, bool preserveClassDraft = false)
    {
        readMapSnapshot();
        if (isRuntime)
        {
            refreshRuntimeActorInfo(preservePositionDraft, preserveClassDraft);
            return;
        }
        MapActorSnapshot? actorData = getActorData();
        if (actorData is null)
            return;
        updatePositionEditors(actorData);
        HeaderChanged?.Invoke(this, EventArgs.Empty);
    }

    private void clearClassDetail()
    {
        defaultValues.Clear();
        fieldsWithDefaults.Clear();
        overriddenFields.Clear();
        FormFields = [];
        FieldsChanged?.Invoke(this, EventArgs.Empty);
        SetClassDetailVisible(false);
    }

    private void updatePositionEditors(MapActorSnapshot actorData)
    {
        MapDocumentSnapshot? map = getMapData();
        int width = Math.Max(1, map?.Width ?? 1);
        int height = Math.Max(1, map?.Height ?? 1);
        MaximumX = width - 1;
        MaximumY = height - 1;
        actorData.TryGetGridPosition(out int x, out int y);
        PositionX = Math.Clamp(x, 0, width - 1);
        PositionY = Math.Clamp(y, 0, height - 1);
    }

    private bool tryGetProjectBlueprintReference(out string reference)
    {
        const string prefix = "Data.Blueprints.";
        reference = blueprintReference ?? string.Empty;
        return reference.StartsWith(prefix, StringComparison.Ordinal)
            && reference.Length > prefix.Length
            && gameData?.Blueprints.BlueprintsData.ContainsKey(reference[prefix.Length..].Replace('.', '/')) == true;
    }

    public void resetOverride(string name)
    {
        readMapSnapshot();
        if (isRuntime || !canEditActor || gameData is null || getEditableActorData() is not MapActorSnapshot actorData
            || getClassVarChanges(actorData) is not JsonObject changes
            || !changes.ContainsKey(name))
        {
            return;
        }
        if (mapKey is null || layerName is null || actorIndex is not int index || actorTag is null
            || !gameData.Maps.RemoveMapActorOverrides(mapKey, layerName, index, actorTag, name))
        {
            refreshActorInfo();
            return;
        }
        RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
        refreshClassDetail();
    }

    public void resetAllOverrides()
    {
        readMapSnapshot();
        if (isRuntime || !canEditActor || gameData is null || getEditableActorData() is not MapActorSnapshot actorData
            || getClassVarChanges(actorData) is not JsonObject changes
            || changes.Count == 0)
        {
            return;
        }
        if (mapKey is null || layerName is null || actorIndex is not int index || actorTag is null
            || !gameData.Maps.RemoveMapActorOverrides(mapKey, layerName, index, actorTag))
        {
            refreshActorInfo();
            return;
        }
        RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
        refreshClassDetail();
    }

    public void refreshClassDetail()
    {
        readMapSnapshot();
        if (isRuntime)
        {
            refreshRuntimeClassDetail();
            return;
        }
        MapActorSnapshot? actorData = getActorData();
        string? reference = actorData?.Blueprint;
        if (actorData is null || string.IsNullOrWhiteSpace(reference)
            || classResolver is null || metadataService is null || fieldBuilder is null)
        {
            clearClassDetail();
            return;
        }

        JsonObject? overrides = getClassVarChanges(actorData);
        ResolvedBlueprintClass resolved = classResolver.Resolve(reference, overrides);
        bool knownClass = resolved.Fields.Count != 0
            || resolved.RootType is not null && metadataService.GetType(resolved.RootType) is not null;
        if (!knownClass)
        {
            clearClassDetail();
            return;
        }

        defaultValues.Clear();
        fieldsWithDefaults.Clear();
        overriddenFields.Clear();
        if (overrides is not null)
        {
            foreach (string name in overrides.Select(pair => pair.Key))
                overriddenFields.Add(name);
        }
        List<BlueprintVariableField> formFields = [];
        foreach (BlueprintVariableField formField in fieldBuilder.Build(resolved))
        {
            if (string.Equals(formField.Name, "tag", StringComparison.Ordinal)
                || isBlueprintOnly(formField.Meta["BlueprintOnly"]))
                continue;
            formFields.Add(formField);
            if (resolved.GetField(formField.Name)?.HasBlueprintDefaultValue == true)
            {
                fieldsWithDefaults.Add(formField.Name);
                defaultValues[formField.Name] = cloneNode(formField.DefaultValue);
            }
        }

        FormFields = formFields;
        FieldsChanged?.Invoke(this, EventArgs.Empty);
        OverridesChanged?.Invoke(this, EventArgs.Empty);
        SetClassDetailVisible(true);
    }

    private static bool isBlueprintOnly(JsonNode? value)
    {
        return value is JsonValue scalar
            && scalar.TryGetValue(out bool boolean)
            && boolean;
    }

    public void SetTag(string nextTag)
    {
        readMapSnapshot();
        if (isRuntime || !canEditActor || gameData is null
            || mapKey is null || layerName is null || actorIndex is not int index || actorTag is null)
            return;
        string oldTag = actorTag;
        string? tag = gameData.Maps.RenameMapActorTag(mapKey, layerName, index, oldTag, nextTag);
        if (tag is null)
        {
            refreshActorInfo();
            return;
        }
        actorTag = tag;
        Tag = tag;
        readMapSnapshot();
        HeaderChanged?.Invoke(this, EventArgs.Empty);
        if (string.Equals(oldTag, tag, StringComparison.Ordinal))
            return;
        RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
        ActorTagChanged?.Invoke(this, EventArgs.Empty);
    }

    public void SetVariable(string name, JsonNode? nextValue, bool requiresRefresh)
    {
        readMapSnapshot();
        if (isRuntime)
        {
            setRuntimeActorVariable(name, nextValue);
            return;
        }
        if (!canEditActor || gameData is null
            || mapKey is null || layerName is null || actorIndex is not int index || actorTag is null)
            return;
        MapActorSnapshot? actorData = getEditableActorData();
        if (actorData is null)
            return;
        JsonNode? value = cloneNode(nextValue);
        bool isDefault = fieldsWithDefaults.Contains(name)
            && blueprintValuesEqual(value, defaultValues.GetValueOrDefault(name));
        JsonObject? changes = getClassVarChanges(actorData);
        bool currentExists = changes?.ContainsKey(name) == true;
        if (isDefault && !currentExists)
            return;
        if (!isDefault && currentExists
            && blueprintValuesEqual(changes![name], value))
        {
            return;
        }

        bool updated = isDefault
            ? gameData.Maps.RemoveMapActorOverrides(mapKey, layerName, index, actorTag, name)
            : editingContext?.SetActorVariable(mapKey, layerName, actorTag, name, value) == true;
        if (!updated)
        {
            refreshActorInfo();
            return;
        }
        if (isDefault)
        {
            overriddenFields.Remove(name);
        }
        else
        {
            overriddenFields.Add(name);
        }
        RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
        OverridesChanged?.Invoke(this, EventArgs.Empty);
        if (requiresRefresh)
            ClassRefreshRequested?.Invoke(this, EventArgs.Empty);
    }

    public void SetPosition(int x, int y)
    {
        readMapSnapshot();
        if (!canMoveActor || editingContext is null
            || mapKey is null || layerName is null || actorIndex is not int index || actorTag is null)
            return;
        MapActorSnapshot? actorData = getEditableActorData();
        if (actorData is null)
            return;
        if (actorData.Position.X == x && actorData.Position.Y == y)
        {
            return;
        }
        string actorId = isRuntime ? runtimeActorId ?? string.Empty : actorTag;
        if (actorId.Length == 0 || !editingContext.MoveActor(mapKey, layerName, actorId, x, y))
            refreshActorInfo();
        else
            RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
    }

    private MapActorSnapshot? getActorData()
    {
        if (mapKey is null || layerName is null || actorIndex is not int index)
            return null;
        MapDocumentSnapshot? map = getMapData();
        if (map is null || !map.Actors.TryGetValue(layerName, out IReadOnlyList<MapActorSnapshot>? actors)
            || index < 0 || index >= actors.Count)
        {
            return null;
        }
        return actors[index];
    }

    private MapActorSnapshot? getEditableActorData()
    {
        MapActorSnapshot? actor = getActorData();
        string? identity = isRuntime ? runtimeActorId : actorTag;
        if (actor is not null && identity is not null
            && string.Equals((isRuntime ? actor.RuntimeId : actor.Tag) ?? string.Empty, identity, StringComparison.Ordinal))
        {
            return actor;
        }
        refreshActorInfo();
        return null;
    }

    private JsonObject? getClassVarChanges(MapActorSnapshot actorData)
    {
        string tag = (isRuntime ? actorData.RuntimeId : actorData.Tag) ?? string.Empty;
        return getMapData()?.ReadActorOverrides(tag);
    }

    private void refreshActorInfo()
    {
        readMapSnapshot();
        setActor(mapKey ?? string.Empty, layerName, actorIndex, getActorData());
    }

    private static bool blueprintValuesEqual(JsonNode? left, JsonNode? right)
    {
        return JsonNode.DeepEquals(left, right);
    }

    private static JsonNode? cloneNode(JsonNode? value)
    {
        return value?.DeepClone();
    }

    private void refreshRuntimeClassDetail()
    {
        if (runtimeActorId is null || fieldBuilder is null
            || getMapData()?.ReadRuntimeInfo(runtimeActorId) is not JsonObject info
            || info["schema"] is not JsonObject schema)
        {
            clearClassDetail();
            displayedRuntimeValues = null;
            displayedRuntimeSchema = null;
            return;
        }
        JsonObject values = getMapData()?.ReadActorOverrides(runtimeActorId)
            ?? info["values"] as JsonObject ?? [];
        if (JsonNode.DeepEquals(displayedRuntimeValues, values) && JsonNode.DeepEquals(displayedRuntimeSchema, schema))
            return;
        string reference = getActorData()?.Type ?? blueprintReference ?? "Engine.Actor";
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
            false, false, false, [], [], null, 0, 0, new HashSet<string>(StringComparer.Ordinal));
        defaultValues.Clear();
        fieldsWithDefaults.Clear();
        overriddenFields.Clear();
        Dictionary<string, BlueprintVariableField> availableFields = new(readOnlyFields, StringComparer.Ordinal);
        foreach (BlueprintVariableField field in fieldBuilder.Build(resolved))
            availableFields[field.Name] = field;
        List<BlueprintVariableField> formFields = [];
        foreach (KeyValuePair<string, JsonNode?> entry in schema)
            if (availableFields.TryGetValue(entry.Key, out BlueprintVariableField? field))
                formFields.Add(field);
        FormFields = formFields;
        FieldsChanged?.Invoke(this, EventArgs.Empty);
        displayedRuntimeValues = values.DeepClone() as JsonObject;
        displayedRuntimeSchema = schema.DeepClone() as JsonObject;
        OverridesChanged?.Invoke(this, EventArgs.Empty);
        SetClassDetailVisible(formFields.Count != 0);
    }

    private void setRuntimeActorVariable(string name, JsonNode? nextValue)
    {
        if (!canEditActor || editingContext is null || mapKey is null || layerName is null
            || runtimeActorId is null || isRuntimeIdentityField(name)
            || getMapData()?.ReadRuntimeInfo(runtimeActorId)?["schema"]?[name] is not JsonObject descriptor
            || descriptor["readOnly"]?.GetValue<bool>() == true
            || descriptor["component"]?.GetValue<bool>() == true)
            return;
        MapActorSnapshot? actor = getEditableActorData();
        JsonObject? values = actor is null ? null : getClassVarChanges(actor);
        if (values is null || !values.ContainsKey(name)
            || (descriptor["component"]?.GetValue<bool>() ?? false) && (values[name] is null || nextValue is null))
            return;
        displayedRuntimeValues = null;
        if (!editingContext.SetActorVariable(mapKey, layerName, runtimeActorId, name, cloneNode(nextValue)))
            refreshRuntimeClassDetail();
    }
}
