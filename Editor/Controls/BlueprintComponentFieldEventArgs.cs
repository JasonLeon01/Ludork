using Ludork.Models;
using System;

namespace Ludork.Controls;

public sealed class BlueprintComponentFieldEventArgs(
    BlueprintVariableField field) : EventArgs
{
    public BlueprintVariableField Field { get; } = field;
}
