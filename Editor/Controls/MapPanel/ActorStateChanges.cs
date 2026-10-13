using Ludork.Models;
using System;
using System.Collections.Generic;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private void updateActorRenderStates(MapDataEditedEventArgs edit)
    {
        if (actorRenderStatesDirty)
            return;
        HashSet<string> changedTags = new(StringComparer.Ordinal);
        foreach (JsonDataEdit change in edit.Edits)
        {
            if (change.Path[0] is "actors")
            {
                if (change.Kind == JsonDataEdit.Operation.Set && change.Path.Count >= 4
                    && change.Path[3] is "position")
                    continue;
                invalidateActorRenderStates();
                return;
            }
            if (change.Path[0] is "BPClassVarChanged")
            {
                if (change.Path.Count < 2 || change.Path[1] is not string tag)
                {
                    invalidateActorRenderStates();
                    return;
                }
                changedTags.Add(tag);
            }
            else if (change.Path[0] is "runtimeInfo")
            {
                invalidateActorRenderStates();
                return;
            }
        }
        if (changedTags.Count == 0)
            return;
        using IDisposable? resolutionBatch = previewService?.BeginResolutionBatch();
        foreach (List<ActorRenderState> states in actorRenderStates.Values)
        {
            for (int index = 0; index < states.Count; index++)
            {
                ActorRenderState state = states[index];
                if (!changedTags.Contains(state.Actor.Tag))
                    continue;
                disposeActorPreviewLease(state.PreviewLease);
                states[index] = createActorRenderState(state.Actor);
            }
        }
        scheduleActorPreviewActivityUpdate();
    }
}
