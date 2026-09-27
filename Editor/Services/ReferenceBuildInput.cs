using Ludork.Models;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed record ReferenceBuildInput(string ProjectPath, long Version, long SchemaVersion,
    IReadOnlyList<ReferenceBuildDocument> Documents);

internal sealed record ReferenceBuildDocument(string Section, string Key, string Path, JsonObject? Data, MapCatalogEntry? Catalog = null);
