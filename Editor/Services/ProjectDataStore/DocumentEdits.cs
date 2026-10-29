using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal bool ApplyDocumentEdits(string section, string key, IReadOnlyList<JsonDataEdit> edits,
        bool contentChanged = true, object? origin = null, bool recordMapHistory = false,
        Action<IReadOnlyList<JsonDataEdit>>? beforeNotify = null)
    {
        EditorDocument? document = GetDocument(section, key);
        if (document is null || edits.Count == 0)
            return false;
        Documents.PrepareGestureDocument(document);
        if (document.InternalData is not JsonObject data)
            return false;
        EditorDocumentState? pending = document.PendingState;
        EditorDocumentState? current = document.CurrentState;
        string? description = document.PendingDescription;
        HistoryMarker? marker = document.PendingMarker;
        long gesture = document.PendingGestureId;
        MapHistoryEdits? mapEdits = document.PendingMapEdits;
        List<JsonDataEdit> changed = [];
        List<IReadOnlyList<JsonDataEdit>> inverse = [];
        bool applied = false;
        using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
        try
        {
            foreach (JsonDataEdit edit in edits)
            {
                if (!edit.CanApply(data))
                    return false;
                if (!edit.Changes(data))
                    continue;
                if (changed.Count == 0)
                    RecordDocumentSnapshot(section, document.Key,
                        mapEdits: recordMapHistory ? MapHistoryEdits.Empty : null);
                inverse.Add(edit.CaptureInverse(data));
                edit.Apply(data);
                changed.Add(edit);
            }
            if (changed.Count == 0)
                return false;
            JsonDataEdit[] forward = changed.ToArray();
            if (recordMapHistory)
                Documents.AppendPendingMapEdits(document, new MapHistoryEdits(forward,
                    inverse.AsEnumerable().Reverse().SelectMany(batch => batch)));
            applied = true;
            beforeNotify?.Invoke(forward);
            Documents.Commit(document, data, change: new EditorDocumentLiveChange(contentChanged, forward, origin));
            Documents.AfterChangeNotifications(refreshDocumentStatus);
            notifications.Commit();
            return true;
        }
        finally
        {
            if (!applied)
            {
                for (int index = inverse.Count - 1; index >= 0; index--)
                    foreach (JsonDataEdit edit in inverse[index])
                        edit.Apply(data);
                document.PendingState = pending;
                document.CurrentState = current;
                document.PendingDescription = description;
                document.PendingMarker = marker;
                document.PendingGestureId = gesture;
                document.PendingMapEdits = mapEdits;
            }
        }
    }
}
