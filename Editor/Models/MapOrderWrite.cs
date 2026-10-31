using System.Collections.Generic;

namespace Ludork.Models;

public sealed record MapOrderWrite(string Scope, IReadOnlyList<MapOrderNode> Tree, string Text, string PreviousText);
