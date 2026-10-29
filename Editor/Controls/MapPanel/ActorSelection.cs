using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private readonly List<MapActorReference> selectedActors = [];
    private MapActorReference? primarySelectedActor;
    private long actorSelectionRevision;

    public IReadOnlyList<MapActorReference> SelectedActors => selectedActors.AsReadOnly();
    public MapActorReference? PrimarySelectedActor => primarySelectedActor;
    public MapActorReference? ActorSelectionAnchor { get; private set; }
    internal event EventHandler? ActorSelectionCommitted;

    public void SelectActors(IReadOnlyList<MapActorReference> actors, MapActorReference? primary,
        MapActorReference? anchor)
    {
        if (IsRuntimeEditing || EditMode != MapEditMode.Actor)
            return;
        setProjectActorSelection(actors, primary, anchor);
    }

    private int? findProjectActor(MapActorReference reference)
    {
        if (getActorList(reference.LayerName) is not IReadOnlyList<MapActorSnapshot> actors)
            return null;
        for (int index = 0; index < actors.Count; index++)
            if (string.Equals(actors[index].Tag, reference.Tag, StringComparison.Ordinal))
                return index;
        return null;
    }

    private void setProjectActorSelection(IReadOnlyList<MapActorReference> actors,
        MapActorReference? primary, MapActorReference? anchor, bool notify = true, bool force = false)
    {
        MapActorReference[] next = actors.Where(actor => actor.LayerName == selectedLayerName
            && findProjectActor(actor) is not null).Distinct().ToArray();
        primary = primary is not null && next.Contains(primary) ? primary : next.LastOrDefault();
        bool changed = !selectedActors.SequenceEqual(next) || primarySelectedActor != primary;
        if (changed)
        {
            actorSelectionRevision++;
            cancelMapGesture();
        }
        selectedActors.Clear();
        selectedActors.AddRange(next);
        primarySelectedActor = primary;
        ActorSelectionAnchor = anchor is not null && findProjectActor(anchor) is not null
            && anchor.LayerName == selectedLayerName ? anchor : primary;
        selectedActorLayer = primary?.LayerName;
        selectedActorIndex = primary is null ? null : findProjectActor(primary);
        selectedRuntimeActorId = null;
        InvalidateVisual();
        if (notify && (changed || force))
            ActorSelectionChanged?.Invoke(this, new ActorSelectionChangedEventArgs(
                CurrentMapKey ?? string.Empty, selectedActorLayer, selectedActorIndex,
                getSelectedActor()?.ToJson(), next, primary, force));
    }

    private void reconcileProjectActorSelection(bool force = false)
    {
        if (!IsRuntimeEditing)
            setProjectActorSelection(selectedActors.ToArray(), primarySelectedActor, ActorSelectionAnchor, true, force);
    }

    private Dictionary<MapActorReference, int> captureSelectedActorRenames(MapDataEditedEventArgs edit)
    {
        Dictionary<MapActorReference, int> renamed = [];
        if (IsRuntimeEditing)
            return renamed;
        foreach (JsonDataEdit change in edit.Edits)
        {
            if (change.Kind != JsonDataEdit.Operation.Set || change.Path.Count != 4
                || change.Path[0] is not "actors" || change.Path[1] is not string layer
                || change.Path[2] is not int index || change.Path[3] is not "tag"
                || getActorList(layer) is not IReadOnlyList<MapActorSnapshot> actors
                || index < 0 || index >= actors.Count)
                continue;
            MapActorReference previous = new(layer, actors[index].Tag);
            if (selectedActors.Contains(previous))
                renamed[previous] = index;
        }
        return renamed;
    }

    private void remapSelectedActorTags(IReadOnlyDictionary<MapActorReference, int> renamed)
    {
        foreach (KeyValuePair<MapActorReference, int> rename in renamed)
        {
            MapActorReference previous = rename.Key;
            if (getActorList(previous.LayerName)?[rename.Value] is not MapActorSnapshot actor)
                continue;
            MapActorReference next = new(previous.LayerName, actor.Tag);
            for (int item = 0; item < selectedActors.Count; item++)
                if (selectedActors[item] == previous)
                    selectedActors[item] = next;
            if (primarySelectedActor == previous)
                primarySelectedActor = next;
            if (ActorSelectionAnchor == previous)
                ActorSelectionAnchor = next;
        }
    }
}
