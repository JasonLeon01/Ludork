using System;
using System.Collections.Generic;
using Ludork.Models;

namespace Ludork.Services;

public sealed record GameVariableSaveResult(bool Success, string Detail)
{
    public static GameVariableSaveResult Completed(string detail) => new(true, detail);
    public static GameVariableSaveResult Failed(string detail) => new(false, detail);
}

public interface IGameVariableCatalog
{
    IReadOnlyList<GameVariableDefinition> Variables { get; }
    bool IsModified { get; }
    event EventHandler? Changed;
    event EventHandler? Saved;
    bool TryGet(string name, out GameVariableDefinition? definition);
}
