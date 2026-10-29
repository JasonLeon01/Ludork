using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public sealed partial class ActorInfoViewModel
{
    private readonly Dictionary<string, ProjectActorFields> projectFieldCache = new(StringComparer.Ordinal);
    private IReadOnlyList<ProjectActorFields> cachedProjectFields = [];
    private long fieldResolverRevision = -1;
    private long fieldMetadataRevision = -1;
    internal event EventHandler? FieldValuesChanged;

    private static bool isTransformField(string name)
        => name is "defaultOrigin" or "defaultTranslation" or "defaultScale" or "defaultRotation";

    private void cacheProjectFields(IReadOnlyList<ProjectActorFields> selections)
    {
        projectFieldCache.Clear();
        foreach (ProjectActorFields selection in selections)
            projectFieldCache.Add(selection.Reference.Tag, selection);
        cachedProjectFields = selections;
        fieldResolverRevision = selections.Count == 0 ? -1 : selections[0].Resolved.ResolverRevision;
        fieldMetadataRevision = selections.Count == 0 ? -1 : selections[0].Resolved.MetadataRevision;
    }

    private bool isProjectFieldCacheCurrent()
        => !isRuntime && cachedProjectFields.Count != 0 && classResolver is not null && metadataService is not null
            && classResolver.Revision == fieldResolverRevision && metadataService.CacheRevision == fieldMetadataRevision;

    internal void ApplyActorDataChanged(MapPreviewChangedEventArgs args,
        bool preservePositionDraft = false, bool preserveClassDraft = false)
    {
        if (mapKey is null || args.MapKey is not null && !string.Equals(mapKey, args.MapKey, StringComparison.Ordinal))
            return;
        if (!isRuntime && args.Edit is { ChangesOnlyActorOverrides: true } edits)
        {
            if (mapSnapshot is not null)
            {
                MapDocumentCodec.ApplyEdits(mapSnapshot, edits);
                if (TryApplyTransformEdits(edits))
                    return;
                if (preserveClassDraft && edits.Edits.All(edit => edit.Path.Count >= 3)
                    && edits.Edits.Any(edit => edit.Path[2] is string name && !isTransformField(name)))
                {
                    cacheProjectFields([]);
                    return;
                }
            }
            refreshClassDetail();
            return;
        }
        refreshActorPosition(preservePositionDraft, preserveClassDraft);
    }

    internal bool TryApplyTransformEdits(MapDataEditedEventArgs edits)
    {
        if (mapSnapshot is null || !isProjectFieldCacheCurrent()
            || edits.Edits.Any(edit => edit.Path.Count != 3 || edit.Path[0] is not "BPClassVarChanged"
                || edit.Path[1] is not string || edit.Path[2] is not string name || !isTransformField(name)
                || edit.Kind is not (JsonDataEdit.Operation.Set or JsonDataEdit.Operation.Remove)))
            return false;
        HashSet<string> changedNames = new(StringComparer.Ordinal);
        foreach (JsonDataEdit edit in edits.Edits)
        {
            if (!projectFieldCache.TryGetValue((string)edit.Path[1], out ProjectActorFields? selection))
                continue;
            string name = (string)edit.Path[2];
            BlueprintVariableField? field = selection.Fields.FirstOrDefault(field => field.Name == name);
            ResolvedBlueprintField? resolved = selection.Resolved.GetField(name);
            if (field is null || resolved is null || !resolved.HasBlueprintDefaultValue)
                return false;
            JsonObject? overrides = mapSnapshot.Data["BPClassVarChanged"]?[selection.Reference.Tag] as JsonObject;
            bool exists = overrides?.ContainsKey(name) == true;
            JsonNode? value = exists ? overrides![name] : resolved.BlueprintDefaultValue;
            if (!isTransformValue(name, value))
                return false;
            field.Value = value?.DeepClone();
            field.DisplayValue = value?.DeepClone();
            field.PreserveNullValue = value is null;
            changedNames.Add(name);
            if (!IsMultipleSelection)
            {
                if (exists)
                    overriddenFields.Add(name);
                else
                    overriddenFields.Remove(name);
            }
        }
        if (changedNames.Count == 0)
            return true;
        if (IsMultipleSelection)
        {
            HashSet<string> affected = new(changedNames, StringComparer.Ordinal);
            foreach (BlueprintVariableField field in FormFields)
                if (BlueprintMultiSelectionFields.GetDependencySource(field) is string source && changedNames.Contains(source))
                    affected.Add(field.Name);
            IReadOnlyList<BlueprintVariableField> replacements = BlueprintMultiSelectionFields.Build(
                cachedProjectFields.Select(selection => selection.Fields).ToArray(), affected);
            Dictionary<string, BlueprintVariableField> byName = replacements.ToDictionary(field => field.Name, StringComparer.Ordinal);
            HashSet<string> previousNames = new(FormFields.Select(field => field.Name), StringComparer.Ordinal);
            if (byName.Keys.Any(name => !previousNames.Contains(name))
                || affected.Any(name => previousNames.Contains(name) && !byName.ContainsKey(name)))
                return false;
            FormFields = FormFields.Select(field => byName.GetValueOrDefault(field.Name) ?? field).ToArray();
            updateProjectOverriddenFields(cachedProjectFields, affected);
        }
        FieldValuesChanged?.Invoke(this, EventArgs.Empty);
        OverridesChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    private static bool isTransformValue(string name, JsonNode? value)
        => name == "defaultRotation" ? JsonScalar.TryGetFiniteNumber(value, out _)
            : value is JsonArray { Count: 2 } vector && JsonScalar.TryGetFiniteNumber(vector[0], out _)
                && JsonScalar.TryGetFiniteNumber(vector[1], out _);

    private bool trySetCachedTransform(string name, JsonNode? nextValue, IReadOnlyList<string> path)
    {
        if (!isTransformField(name) || !isProjectFieldCacheCurrent() || mapSnapshot is null)
            return false;
        if (!canEditActor || gameData is null || mapKey is null)
            return true;
        BlueprintVariableField? common = FormFields.FirstOrDefault(field => field.Name == name);
        if (common is null)
            return false;
        if (common.IsReadOnly || IsMultipleSelection
            && !BlueprintMultiSelectionFields.EditableLeaves(common).Any(leaf => leaf.EditPath.SequenceEqual(path)))
            return true;
        List<MapActorOverrideEdit> edits = [];
        bool changed = false;
        foreach (ProjectActorFields selection in cachedProjectFields)
        {
            BlueprintVariableField? field = selection.Fields.FirstOrDefault(field => field.Name == name);
            ResolvedBlueprintField? resolved = selection.Resolved.GetField(name);
            if (field is null || resolved is null)
                return false;
            JsonNode? value = BlueprintMultiSelectionFields.Replace(BlueprintMultiSelectionFields.EffectiveValue(field), path, nextValue);
            bool remove = resolved.HasBlueprintDefaultValue && JsonNode.DeepEquals(value, resolved.BlueprintDefaultValue);
            JsonObject? overrides = mapSnapshot.Data["BPClassVarChanged"]?[selection.Reference.Tag] as JsonObject;
            bool exists = overrides?.ContainsKey(name) == true;
            changed |= remove ? exists : !exists || !JsonNode.DeepEquals(overrides![name], value);
            edits.Add(new MapActorOverrideEdit(selection.Reference, name, value, remove));
        }
        if (changed && !gameData.Maps.ApplyMapActorOverrides(mapKey, edits))
            refreshActorInfo();
        return true;
    }
}
