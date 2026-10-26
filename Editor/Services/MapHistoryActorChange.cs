using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed record MapHistoryActorChange(string Layer, int Index, JsonObject Actor,
    JsonObject? Overrides, IReadOnlySet<string>? Fields);
