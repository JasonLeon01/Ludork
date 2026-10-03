using System.Collections.Generic;

namespace Ludork.Models;

public sealed record LuaEnumDefinition(
    string ModuleName,
    LuaMetadataType? ValueType,
    IReadOnlyList<BlueprintVariableOption> Options,
    string? Error)
{
    public bool IsValid => Error is null && ValueType is not null && Options.Count > 0;
}
