using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public sealed partial class ActorInfoViewModel
{
    private IReadOnlyList<MapActorReference> projectActors = [];
    private MapActorReference? projectPrimary;

    public bool IsMultipleSelection => projectActors.Count > 1;
    public bool IsPositionXMixed { get; private set; }
    public bool IsPositionYMixed { get; private set; }
    public bool IsBlueprintMixed { get; private set; }

    public void setActors(string nextMapKey, IReadOnlyList<MapActorReference> actors, MapActorReference? primary,
        bool preservePositionDraft = false, bool preserveClassDraft = false)
    {
        if (isRuntime)
            return;
        bool sameSelection = mapKey == nextMapKey && projectActors.Count == actors.Count
            && projectActors.All(actors.Contains);
        mapKey = string.IsNullOrWhiteSpace(nextMapKey) ? null : nextMapKey;
        readMapSnapshot();
        string? selectedLayer = primary?.LayerName ?? actors.FirstOrDefault()?.LayerName;
        List<MapActorReference> validActors = actors.Where(actor => actor.LayerName == selectedLayer
            && findProjectActor(actor) is not null).Distinct().ToList();
        MapActorReference? selectedPrimary = primary is not null && validActors.Contains(primary)
            ? primary : validActors.LastOrDefault();
        if (validActors.Count == 1 && sameSelection && projectActors.Count == 1)
        {
            projectActors = validActors;
            projectPrimary = selectedPrimary;
            MapActorSnapshot? selectedActor = findProjectActor(validActors[0]);
            if (selectedActor is not null)
            {
                actorIndex = findProjectActorIndex(validActors[0]);
                actorTag = selectedActor.Tag;
                Tag = selectedActor.Tag;
                blueprintReference = selectedActor.Blueprint;
                BlueprintReference = selectedActor.Blueprint;
                if (!preservePositionDraft)
                {
                    updatePositionEditors(selectedActor);
                    HeaderChanged?.Invoke(this, EventArgs.Empty);
                }
                EditableStateChanged?.Invoke(this, EventArgs.Empty);
                if (!preserveClassDraft)
                    refreshClassDetail();
            }
            return;
        }
        if (validActors.Count <= 1)
        {
            MapActorReference? selected = validActors.FirstOrDefault();
            MapActorSnapshot? actor = selected is null ? null : findProjectActor(selected);
            int? index = selected is null ? null : findProjectActorIndex(selected);
            setActor(nextMapKey, selected?.LayerName, index, actor, preservePositionDraft, preserveClassDraft);
            projectActors = validActors;
            projectPrimary = selectedPrimary;
            return;
        }
        projectActors = validActors;
        projectPrimary = selectedPrimary;
        runtimeActorId = null;
        displayedRuntimeValues = null;
        displayedRuntimeSchema = null;
        layerName = selectedLayer;
        actorTag = selectedPrimary?.Tag;
        actorIndex = selectedPrimary is null ? null : findProjectActorIndex(selectedPrimary);
        HasSelection = true;
        if (!sameSelection || !preservePositionDraft)
            updateProjectSelectionHeader();
        EditableStateChanged?.Invoke(this, EventArgs.Empty);
        if (!sameSelection || !preserveClassDraft)
            refreshProjectSelectionFields();
    }

    public void SetPositionAxis(bool xAxis, int value)
    {
        readMapSnapshot();
        if (projectActors.Count <= 1)
        {
            MapActorSnapshot? actor = getActorData();
            if (actor?.TryGetGridPosition(out int x, out int y) == true)
                SetPosition(xAxis ? value : x, xAxis ? y : value);
            return;
        }
        readMapSnapshot();
        if (!canMoveActor || gameData is null || mapKey is null)
            return;
        List<MapActorPositionEdit> edits = [];
        foreach (MapActorReference reference in projectActors)
        {
            MapActorSnapshot? actor = findProjectActor(reference);
            if (actor is null || !actor.TryGetGridPosition(out int x, out int y))
            {
                refreshActorInfo();
                return;
            }
            edits.Add(new MapActorPositionEdit(reference, xAxis ? value : x, xAxis ? y : value));
        }
        if (!gameData.Maps.MoveMapActors(mapKey, edits))
            refreshActorInfo();
        else
        {
            readMapSnapshot();
            updateProjectSelectionHeader();
            RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
        }
    }

    private void refreshProjectSelectionHeader()
    {
        List<MapActorReference> remaining = projectActors.Where(actor => findProjectActor(actor) is not null).ToList();
        if (remaining.Count != projectActors.Count)
        {
            setActors(mapKey ?? string.Empty, remaining, projectPrimary);
            return;
        }
        updateProjectSelectionHeader();
    }

    private void updateProjectSelectionHeader()
    {
        List<MapActorSnapshot> actors = projectActors.Select(findProjectActor).OfType<MapActorSnapshot>().ToList();
        if (actors.Count == 0)
            return;
        MapActorSnapshot actor = projectPrimary is null ? actors[0] : findProjectActor(projectPrimary) ?? actors[0];
        updatePositionEditors(actor);
        IsPositionXMixed = actors.Any(item => item.Position.X != actor.Position.X);
        IsPositionYMixed = actors.Any(item => item.Position.Y != actor.Position.Y);
        IsBlueprintMixed = actors.Any(item => item.Blueprint != actor.Blueprint);
        blueprintReference = IsBlueprintMixed ? null : actor.Blueprint;
        BlueprintReference = IsBlueprintMixed ? LocaleService.Get("MULTIPLE_CHOICES") : actor.Blueprint;
        Tag = LocaleService.Get("MULTIPLE_CHOICES");
        HeaderChanged?.Invoke(this, EventArgs.Empty);
    }

    private MapActorSnapshot? findProjectActor(MapActorReference reference)
        => mapSnapshot?.Actors.TryGetValue(reference.LayerName, out IReadOnlyList<MapActorSnapshot>? actors) == true
            ? actors.FirstOrDefault(actor => actor.Tag == reference.Tag) : null;

    private int? findProjectActorIndex(MapActorReference reference)
    {
        if (mapSnapshot is null || !mapSnapshot.Actors.TryGetValue(reference.LayerName, out IReadOnlyList<MapActorSnapshot>? actors))
            return null;
        for (int index = 0; index < actors.Count; index++)
            if (actors[index].Tag == reference.Tag)
                return index;
        return null;
    }

    private List<ProjectActorFields>? readProjectSelectionFields()
    {
        if (classResolver is null || fieldBuilder is null)
            return null;
        List<ProjectActorFields> result = [];
        foreach (MapActorReference reference in projectActors)
        {
            MapActorSnapshot? actor = findProjectActor(reference);
            if (actor is null)
                return null;
            ResolvedBlueprintClass resolved = classResolver.Resolve(actor.Blueprint, mapSnapshot?.ReadActorOverrides(actor.Tag));
            IReadOnlyList<BlueprintVariableField> fields = fieldBuilder.Build(resolved)
                .Where(field => field.Name != "tag" && !isBlueprintOnly(field.Meta["BlueprintOnly"])).ToArray();
            result.Add(new ProjectActorFields(reference, resolved, fields));
        }
        return result;
    }

    private void refreshProjectSelectionFields()
    {
        List<ProjectActorFields>? selections = readProjectSelectionFields();
        if (selections is null || selections.Count != projectActors.Count)
        {
            clearClassDetail();
            return;
        }
        FormFields = BlueprintMultiSelectionFields.Build(selections.Select(selection => selection.Fields).ToArray());
        defaultValues.Clear();
        fieldsWithDefaults.Clear();
        updateProjectOverriddenFields(selections);
        FieldsChanged?.Invoke(this, EventArgs.Empty);
        OverridesChanged?.Invoke(this, EventArgs.Empty);
        SetClassDetailVisible(FormFields.Count > 0);
    }

    private void updateProjectOverriddenFields(IReadOnlyList<ProjectActorFields> selections)
    {
        overriddenFields.Clear();
        foreach (BlueprintVariableField common in FormFields)
        {
            IReadOnlyList<BlueprintVariableField> leaves = BlueprintMultiSelectionFields.EditableLeaves(common).ToArray();
            if (leaves.Count == 0)
                continue;
            foreach (ProjectActorFields selection in selections)
            {
                if (mapSnapshot?.ReadActorOverrides(selection.Reference.Tag)?.ContainsKey(common.Name) != true)
                    continue;
                ResolvedBlueprintField? resolved = selection.Resolved.GetField(common.Name);
                BlueprintVariableField? field = selection.Fields.FirstOrDefault(item => item.Name == common.Name);
                JsonNode? current = field is null ? null : BlueprintMultiSelectionFields.EffectiveValue(field);
                if (leaves.Any(leaf => leaf.EditPath.Count == 0
                    || !BlueprintMultiSelectionFields.TryRead(resolved?.BlueprintDefaultValue, leaf.EditPath, out JsonNode? defaultValue)
                    || !BlueprintMultiSelectionFields.TryRead(current, leaf.EditPath, out JsonNode? currentValue)
                    || !JsonNode.DeepEquals(currentValue, defaultValue)))
                {
                    overriddenFields.Add(common.Name);
                    break;
                }
            }
        }
    }

    private void setProjectSelectionVariable(string name, JsonNode? nextValue, IReadOnlyList<string> path, bool requiresRefresh)
    {
        readMapSnapshot();
        if (!canEditActor || gameData is null || mapKey is null)
            return;
        List<ProjectActorFields>? selections = readProjectSelectionFields();
        if (selections is null)
        {
            refreshActorInfo();
            return;
        }
        IReadOnlyList<BlueprintVariableField> commonFields = BlueprintMultiSelectionFields.Build(selections.Select(selection => selection.Fields).ToArray());
        BlueprintVariableField? common = commonFields.FirstOrDefault(field => field.Name == name);
        if (common is null || !BlueprintMultiSelectionFields.EditableLeaves(common).Any(leaf => leaf.EditPath.SequenceEqual(path)))
            return;
        List<MapActorOverrideEdit> edits = [];
        foreach (ProjectActorFields selection in selections)
        {
            BlueprintVariableField field = selection.Fields.First(item => item.Name == name);
            JsonNode? value = BlueprintMultiSelectionFields.Replace(BlueprintMultiSelectionFields.EffectiveValue(field), path, nextValue);
            ResolvedBlueprintField? resolved = selection.Resolved.GetField(name);
            bool remove = resolved?.HasBlueprintDefaultValue == true && JsonNode.DeepEquals(value, resolved.BlueprintDefaultValue);
            edits.Add(new MapActorOverrideEdit(selection.Reference, name, value, remove));
        }
        if (!gameData.Maps.ApplyMapActorOverrides(mapKey, edits))
        {
            refreshActorInfo();
            return;
        }
        readMapSnapshot();
        List<ProjectActorFields>? updated = readProjectSelectionFields();
        if (updated is not null)
        {
            FormFields = BlueprintMultiSelectionFields.Build(updated.Select(selection => selection.Fields).ToArray());
            updateProjectOverriddenFields(updated);
        }
        RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
        OverridesChanged?.Invoke(this, EventArgs.Empty);
        FieldStatesChanged?.Invoke(this, EventArgs.Empty);
        if (requiresRefresh || name == "material")
            ClassRefreshRequested?.Invoke(this, EventArgs.Empty);
    }

    private void resetProjectSelectionOverrides(string? name)
    {
        readMapSnapshot();
        if (!canEditActor || gameData is null || mapKey is null)
            return;
        List<ProjectActorFields>? selections = readProjectSelectionFields();
        if (selections is null)
        {
            refreshActorInfo();
            return;
        }
        IReadOnlyList<BlueprintVariableField> commonFields = BlueprintMultiSelectionFields.Build(selections.Select(selection => selection.Fields).ToArray());
        List<MapActorOverrideEdit> edits = [];
        foreach (BlueprintVariableField common in commonFields.Where(field => name is null || field.Name == name))
        {
            IReadOnlyList<BlueprintVariableField> leaves = BlueprintMultiSelectionFields.EditableLeaves(common).ToArray();
            if (leaves.Count == 0)
                continue;
            foreach (ProjectActorFields selection in selections)
            {
                IReadOnlyList<BlueprintVariableField> targetFields = BlueprintMultiSelectionFields.Build([selection.Fields]);
                BlueprintVariableField field = targetFields.First(item => item.Name == common.Name);
                ResolvedBlueprintField? resolved = selection.Resolved.GetField(common.Name);
                (JsonNode? value, bool remove) = BlueprintMultiSelectionFields.ResetCommonValue(field, common,
                    resolved?.BlueprintDefaultValue, resolved?.HasBlueprintDefaultValue == true);
                remove |= resolved?.HasBlueprintDefaultValue == true && JsonNode.DeepEquals(value, resolved.BlueprintDefaultValue);
                edits.Add(new MapActorOverrideEdit(selection.Reference, common.Name, value, remove));
            }
        }
        HashSet<MapActorReference> materialChanges = new(edits.Where(edit => edit.Name == "material"
            && !JsonNode.DeepEquals(BlueprintMultiSelectionFields.EffectiveValue(selections.First(selection => selection.Reference == edit.Actor)
                .Fields.First(field => field.Name == "material")), edit.Value)).Select(edit => edit.Actor));
        edits.RemoveAll(edit => edit.Name == "materialParameters" && materialChanges.Contains(edit.Actor));
        if (edits.Count == 0)
            return;
        if (!gameData.Maps.ApplyMapActorOverrides(mapKey, edits))
        {
            refreshActorInfo();
            return;
        }
        RefreshSelectedActorRequested?.Invoke(this, EventArgs.Empty);
        refreshClassDetail();
    }

    private sealed record ProjectActorFields(MapActorReference Reference, ResolvedBlueprintClass Resolved,
        IReadOnlyList<BlueprintVariableField> Fields);
}
