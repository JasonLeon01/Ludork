using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintPreviewService
{
    private readonly Dictionary<string, ResolvedBlueprintClass> batchActorClasses = new(StringComparer.Ordinal);
    private readonly Dictionary<(ResolvedBlueprintClass Class, string Reference), ActorVisualDescriptor?> batchActorVisuals = [];
    private int resolutionBatchDepth;
    private long resolutionBatchRevision = -1;

    public IDisposable BeginResolutionBatch()
    {
        IDisposable metadataRead = classResolver.BeginBatch();
        resolutionBatchDepth++;
        return new ResolutionBatch(this, metadataRead);
    }

    private ResolvedBlueprintClass resolveActorClass(string reference, JsonObject? overrides)
    {
        if (resolutionBatchDepth == 0 || overrides is { Count: > 0 })
            return classResolver.Resolve(reference, overrides);
        ensureResolutionBatchCurrent();
        if (batchActorClasses.TryGetValue(reference, out ResolvedBlueprintClass? cached))
            return cached;
        ResolvedBlueprintClass resolved = classResolver.Resolve(reference, overrides);
        ensureResolutionBatchCurrent();
        batchActorClasses.Add(reference, resolved);
        return resolved;
    }

    private void ensureResolutionBatchCurrent()
    {
        if (resolutionBatchRevision == classResolver.Revision)
            return;
        batchActorClasses.Clear();
        batchActorVisuals.Clear();
        resolutionBatchRevision = classResolver.Revision;
    }

    private sealed class ResolutionBatch(BlueprintPreviewService owner, IDisposable metadataRead) : IDisposable
    {
        private bool disposed;

        public void Dispose()
        {
            if (disposed)
                return;
            disposed = true;
            metadataRead.Dispose();
            if (--owner.resolutionBatchDepth == 0)
            {
                owner.batchActorClasses.Clear();
                owner.batchActorVisuals.Clear();
            }
        }
    }
}
