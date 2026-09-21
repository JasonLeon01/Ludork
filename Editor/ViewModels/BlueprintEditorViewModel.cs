using Ludork.Models;
using Ludork.Services;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

internal sealed class BlueprintEditorViewModel : ViewModelBase, IDisposable
{
    private readonly LuaMetadataService metadata;
    private readonly BlueprintClassResolver resolver;
    private readonly BlueprintValidationService validation;
    private BlueprintNodeDefinitionCatalog? nodeCatalog;
    private BlueprintEditorTabItem? selectedTab;

    public BlueprintEditorViewModel(
        BlueprintEditorDocument document,
        ProjectDataStore data,
        LuaMetadataService metadata,
        BlueprintClassResolver resolver)
    {
        Document = document;
        this.metadata = metadata;
        this.resolver = resolver;
        validation = new BlueprintValidationService(data, metadata, resolver);
        FieldBuilder = new BlueprintVariableFieldBuilder(data, metadata);
    }

    public BlueprintEditorDocument Document { get; }
    public BlueprintVariableFieldBuilder FieldBuilder { get; }
    public ResolvedBlueprintClass? ResolvedClass { get; private set; }
    public ResolvedBlueprintClass? ResolvedParent { get; private set; }
    public string ParentReference => Document.Data["parent"]?.GetValue<string>() ?? string.Empty;
    public bool IsBlueprint => Document.Kind == BlueprintEditorDocumentKind.Blueprint;
    public bool IsGraphReadOnly => IsBlueprint && ResolvedClass?.ScriptMixin == true;
    public bool CanEditAttributes => Document.CanEditAttributes;
    public IReadOnlyList<BlueprintEditorTabItem> Tabs { get; private set; } = [];

    public BlueprintEditorTabItem? SelectedTab
    {
        get => selectedTab;
        set => SetProperty(ref selectedTab, value);
    }

    public ResolvedBlueprintClass ResolveClass()
    {
        ResolvedClass = resolver.ResolveBlueprint(Document.Data, Document.BlueprintKey);
        return ResolvedClass;
    }

    public IReadOnlyList<BlueprintVariableField> RefreshFields()
    {
        ResolvedBlueprintClass resolved = ResolveClass();
        ResolvedParent = ParentReference.Length == 0 ? null : resolver.Resolve(ParentReference);
        OnPropertyChanged(nameof(ParentReference));
        OnPropertyChanged(nameof(IsGraphReadOnly));
        return FieldBuilder.Build(resolved, IsBlueprint);
    }

    public bool SupportsPreview()
    {
        return resolver.IsDerivedFrom(ResolvedClass ?? ResolveClass(), "Engine.Actor");
    }

    public bool HasLocalAttribute(string name)
    {
        return Document.Data["attrs"] is JsonObject attrs && attrs.ContainsKey(name);
    }

    public bool IsGeneralDataSelector(string name)
    {
        return ResolvedClass is not null && FieldBuilder.IsGeneralDataSelector(ResolvedClass, name);
    }

    public bool CanChangeParent(string parent)
    {
        JsonObject prospective = (JsonObject)Document.Data.DeepClone();
        prospective["parent"] = parent;
        ResolvedBlueprintClass next = resolver.ResolveBlueprint(prospective, Document.BlueprintKey);
        return !next.HasBlueprintParent || prospective["attrs"] is not JsonObject attrs
            || !tryGetBoolean(attrs["scriptMixin"], out bool localMode) || localMode == next.ParentScriptMixin;
    }

    public BlueprintAttributeChange PrepareScriptPath(JsonNode? value)
    {
        string candidate = value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : string.Empty;
        string normalized;
        try
        {
            normalized = ScriptMixinPaths.Normalize(candidate);
            string fullPath = ScriptMixinPaths.GetScriptPath(metadata.ProjectPath, normalized);
            if (string.IsNullOrEmpty(normalized) || !File.Exists(fullPath))
                throw new FileNotFoundException($"Mixin script '{normalized}' was not found", fullPath);
            metadata.LoadScriptMixinMetadata(normalized);
        }
        catch (InterpreterException exception)
        {
            return BlueprintAttributeChange.Failed(exception.DecoratedMessage ?? exception.Message);
        }
        catch (InvalidDataException exception)
        {
            return BlueprintAttributeChange.Failed(exception.Message);
        }
        catch (IOException exception)
        {
            return BlueprintAttributeChange.Failed(exception.Message);
        }
        catch (UnauthorizedAccessException exception)
        {
            return BlueprintAttributeChange.Failed(exception.Message);
        }
        return prepareScriptPathChange(JsonValue.Create(normalized), false);
    }

    public BlueprintAttributeChange? PrepareRemoveScriptPath()
    {
        return HasLocalAttribute("scriptPath") ? prepareScriptPathChange(null, true) : null;
    }

    private BlueprintAttributeChange prepareScriptPathChange(JsonNode? value, bool remove)
    {
        ResolvedBlueprintClass previous = ResolvedClass ?? ResolveClass();
        JsonObject prospective = (JsonObject)Document.Data.DeepClone();
        JsonObject prospectiveAttrs = prospective["attrs"] as JsonObject ?? [];
        prospective["attrs"] = prospectiveAttrs;
        if (remove)
            prospectiveAttrs.Remove("scriptPath");
        else
            prospectiveAttrs["scriptPath"] = value;
        ResolvedBlueprintClass next = resolver.ResolveBlueprint(prospective, Document.BlueprintKey);
        HashSet<string> nextSchema = new(next.DeclaredFieldNames, StringComparer.Ordinal);
        List<string> stale = previous.LocalMixinFieldNames
            .Where(name => HasLocalAttribute(name) && !nextSchema.Contains(name))
            .Distinct(StringComparer.Ordinal).ToList();
        Dictionary<string, JsonNode?> updates = new(StringComparer.Ordinal);
        List<string> removals = [.. stale];
        if (remove)
            removals.Insert(0, "scriptPath");
        else
            updates["scriptPath"] = value?.DeepClone();
        return new BlueprintAttributeChange(updates, removals, stale, null);
    }

    public bool CommitAttributeChange(BlueprintAttributeChange change)
    {
        return change.Error is null && Document.CommitAttributes(change.Updates, change.Removals);
    }

    public string? ValidateAttributeName(string name)
    {
        if (name.Length == 0 || char.IsDigit(name[0]))
            return LocaleService.Get("ATTR_NAME_CANNOT_START_WITH_DIGIT");
        return (ResolvedClass ?? ResolveClass()).InvalidVars.Contains(name, StringComparer.Ordinal)
            ? LocaleService.Get("INVALID_NAME")
            : null;
    }

    public bool AddComponent(BlueprintVariableField field) => Document.CommitAttribute(field.Name, materializeComponent(field));
    public bool CommitAttribute(string name, JsonNode? value) => Document.CommitAttribute(name, value);
    public bool RemoveAttribute(string name) => Document.RemoveAttribute(name);
    public bool CommitParent(string parent) => Document.CommitParent(parent);
    public bool AddEvent(string name) => Document.AddEvent(name);
    public bool RenameEvent(string name, string nextName) => Document.RenameEvent(name, nextName);
    public bool DeleteEvent(string name) => Document.DeleteEvent(name);
    public BlueprintValidationResult Validate(string key) => validation.ValidateBlueprint(key);

    public IReadOnlyList<string> GetAvailableGraphNames()
    {
        List<string> result = Document.GetGraphNames().ToList();
        if (!IsBlueprint)
            return result;
        ResolvedBlueprintClass resolved = ResolvedClass ?? ResolveClass();
        if (resolved.RootType is null)
            return result;
        foreach (LuaNodeMemberMetadata member in metadata.GetNodeMembers(resolved.RootType, LuaNodeMemberKind.Event))
        {
            if (!result.Contains(member.Name, StringComparer.Ordinal))
                result.Add(member.Name);
        }
        return result;
    }

    public void RefreshTabs(string? preferredEvent, bool preferPreview)
    {
        string? selectedEvent = preferredEvent ?? SelectedTab?.EventName;
        bool selectedPreview = preferPreview || SelectedTab?.IsPreview == true;
        List<BlueprintEditorTabItem> tabs = [];
        if (SupportsPreview())
            tabs.Add(new BlueprintEditorTabItem(LocaleService.Get("PREVIEW"), null, true));
        foreach (string graph in GetAvailableGraphNames())
            tabs.Add(new BlueprintEditorTabItem(EditorDisplayName.Format(graph), graph, false));
        Tabs = tabs;
        OnPropertyChanged(nameof(Tabs));
        SelectedTab = (selectedPreview ? tabs.FirstOrDefault(tab => tab.IsPreview) : null)
            ?? (selectedEvent is null ? null : tabs.FirstOrDefault(tab => tab.EventName == selectedEvent))
            ?? tabs.FirstOrDefault();
    }

    public BlueprintGraphEditorData LoadGraph(string eventName, JsonObject eventGraph)
    {
        nodeCatalog ??= new BlueprintNodeDefinitionCatalog(metadata, resolver);
        BlueprintNodeDefinitionSet definitions;
        IReadOnlyList<BlueprintGraphEventParameterDefinition> eventParameters;
        using (IDisposable read = resolver.BeginBatch())
        {
            definitions = nodeCatalog.GetNodeDefinitionSet(
                new BlueprintGraphContext(Document.Data, Document.BlueprintKey), ResolvedClass);
            eventParameters = definitions.EventParameters.TryGetValue(eventName,
                out IReadOnlyList<BlueprintGraphEventParameterDefinition>? parameters) ? parameters : [];
        }
        BlueprintGraphDocument graph = BlueprintGraphCodec.Load(
            eventName, eventGraph, Document.Data["graph"]?["startNodes"]?[eventName], definitions, eventParameters);
        IReadOnlyList<BlueprintGraphNodeDefinition> available = Document.IsGraphOnly
            ? definitions.Definitions.Where(definition => !definition.IsLatent).ToArray()
            : definitions.Definitions;
        return new BlueprintGraphEditorData(graph, available);
    }

    public void CommitGraph(string eventName, BlueprintGraphDocument graph)
    {
        Document.CommitEventGraph(eventName, BlueprintGraphCodec.Save(graph));
    }

    public void InvalidateNodeDefinitions() => nodeCatalog?.Invalidate();

    public bool DependsOnChangedAncestor(EditorDocumentsChangedEventArgs args)
    {
        return IsBlueprint && ResolvedClass is not null && args.Changes.Any(change => change.Section == "Blueprints"
            && change.DocumentId != Document.ResourceDocument?.Id
            && (change.Key is string key && ResolvedClass.DependsOnBlueprint(key)
                || change.PreviousKey is string previous && ResolvedClass.DependsOnBlueprint(previous)));
    }

    public void Dispose() => Document.Dispose();

    private static JsonNode materializeComponent(BlueprintVariableField field)
    {
        if (field.Value is not null)
            return field.Value.DeepClone();
        if (field.DefaultValue is not null)
            return field.DefaultValue.DeepClone();
        JsonObject result = [];
        foreach (BlueprintVariableField child in field.Fields)
            result[child.Name] = child.Fields.Count > 0 ? materializeComponent(child)
                : child.Value?.DeepClone() ?? child.DefaultValue?.DeepClone() ?? JsonValue.Create(string.Empty);
        return result;
    }

    private static bool tryGetBoolean(JsonNode? value, out bool result)
    {
        if (value is JsonValue scalar && scalar.TryGetValue(out result))
            return true;
        result = false;
        return false;
    }
}
