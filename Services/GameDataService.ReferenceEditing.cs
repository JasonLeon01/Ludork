using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public bool ApplyReferenceRewrites(IReadOnlyList<ReferenceRewrite> rewrites)
    {
        IReadOnlyList<ReferenceRewrite> prepared = prepareReferenceRewrites(rewrites);
        if (prepared.Count == 0)
            return false;
        recordReferenceSnapshot(prepared);
        applyPreparedReferenceRewrites(prepared);
        notifyReferenceRewrites(prepared);
        refreshModifiedState();
        return true;
    }

    private IReadOnlyList<ReferenceRewrite> prepareReferenceRewrites(IReadOnlyList<ReferenceRewrite> rewrites)
    {
        List<ReferenceRewrite> prepared = [];
        HashSet<(string Section, string Key)> targets = [];
        foreach (ReferenceRewrite rewrite in rewrites)
        {
            if (rewrite.Section is not ("Maps" or "Configs" or "Blueprints" or "CommonFunctions" or "General")
                || !targets.Add((rewrite.Section, rewrite.Key)))
            {
                throw new InvalidDataException($"Invalid reference rewrite target: {rewrite.Section}/{rewrite.Key}.");
            }
            JsonObject? current = rewrite.Section == "Maps"
                ? ReadMapSnapshotWithoutCaching(rewrite.Key)
                : sections[rewrite.Section].Data.GetValueOrDefault(rewrite.Key);
            if (current is null || !JsonNode.DeepEquals(current, rewrite.Original))
                throw new InvalidOperationException($"The reference rewrite target changed: {rewrite.Section}/{rewrite.Key}.");
            if (JsonNode.DeepEquals(current, rewrite.Candidate))
                continue;
            prepared.Add(new ReferenceRewrite(
                rewrite.Section,
                rewrite.Key,
                (JsonObject)rewrite.Original.DeepClone(),
                (JsonObject)rewrite.Candidate.DeepClone()));
        }
        return prepared;
    }

    private void recordReferenceSnapshot(IReadOnlyList<ReferenceRewrite> rewrites)
    {
        if (activeHistoryGestureId != 0 && activeHistoryGestureHasSnapshot)
            return;
        HashSet<string> mapKeys = rewrites.Where(rewrite => rewrite.Section == "Maps")
            .Select(rewrite => rewrite.Key).ToHashSet(StringComparer.Ordinal);
        Dictionary<string, Dictionary<string, JsonObject>> snapshot = cloneAllData(mapKeys);
        foreach (ReferenceRewrite rewrite in rewrites.Where(rewrite => rewrite.Section == "Maps"))
            snapshot["Maps"][rewrite.Key] = (JsonObject)rewrite.Original.DeepClone();
        pushHistorySnapshot(snapshot);
    }

    private void applyPreparedReferenceRewrites(IReadOnlyList<ReferenceRewrite> rewrites)
    {
        foreach (ReferenceRewrite rewrite in rewrites)
        {
            if (rewrite.Section == "Maps" && !sections["Maps"].Data.ContainsKey(rewrite.Key))
                originData["Maps"][rewrite.Key] = (JsonObject)rewrite.Original.DeepClone();
            sections[rewrite.Section].Data[rewrite.Key] = rewrite.Candidate;
            if (rewrite.Section == "Maps")
                updateLoadedMapMetadata(rewrite.Key, rewrite.Candidate);
        }
    }

    private void notifyReferenceRewrites(IReadOnlyList<ReferenceRewrite> rewrites)
    {
        foreach (ReferenceRewrite rewrite in rewrites.Where(rewrite => rewrite.Section == "Maps"))
            NotifyMapContentChanged(rewrite.Key);
    }

    private void applyMutationWithReferences(
        Action mutation,
        Func<IReadOnlyList<ReferenceRewrite>> prepareReferences,
        IReadOnlySet<string> retainedMapKeys)
    {
        IReadOnlyList<ReferenceRewrite> beforeReferences = prepareReferenceRewrites(prepareReferences());
        Dictionary<string, Dictionary<string, JsonObject>> before = cloneAllData(sections["Maps"].Data.Keys.ToHashSet(StringComparer.Ordinal));
        Dictionary<string, Dictionary<string, JsonObject>> originBefore = cloneData(originData);
        IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> undoBefore = cloneHistory(undoStack);
        IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> redoBefore = cloneHistory(redoStack);
        bool gestureHadSnapshot = activeHistoryGestureHasSnapshot;
        bool modifiedBefore = isModified;
        Dictionary<string, Dictionary<string, JsonObject>>? snapshot = null;
        IReadOnlyList<ReferenceRewrite> prepared;
        try
        {
            if (activeHistoryGestureId == 0 || !activeHistoryGestureHasSnapshot)
            {
                snapshot = cloneAllData(retainedMapKeys);
                foreach (ReferenceRewrite rewrite in beforeReferences.Where(rewrite => rewrite.Section == "Maps"))
                    snapshot["Maps"][rewrite.Key] = (JsonObject)rewrite.Original.DeepClone();
            }
            mutation();
            prepared = prepareReferenceRewrites(prepareReferences());
            applyPreparedReferenceRewrites(prepared);
            if (snapshot is not null)
                pushHistorySnapshot(snapshot);
        }
        catch
        {
            originData = originBefore;
            restoreHistory(undoStack, undoBefore);
            restoreHistory(redoStack, redoBefore);
            activeHistoryGestureHasSnapshot = gestureHadSnapshot;
            restoreSnapshot(before, false, false);
            isModified = modifiedBefore;
            throw;
        }
        notifyReferenceRewrites(prepared);
    }
}
