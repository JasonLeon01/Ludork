using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

internal static class SnapshotJson
{
    public static IReadOnlyDictionary<string, JsonObject> ToDictionary<T>(IReadOnlyDictionary<string, T> snapshots)
        where T : IEditorDataSnapshot => new JsonSnapshotProjection<T>(snapshots);
}
