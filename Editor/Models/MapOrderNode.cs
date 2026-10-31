using System.Collections.Generic;

namespace Ludork.Models;

public sealed record MapOrderNode(string Name, IReadOnlyList<MapOrderNode> Children);
