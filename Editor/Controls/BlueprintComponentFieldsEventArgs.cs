using System;
using System.Collections.Generic;

namespace Ludork.Controls;

public sealed class BlueprintComponentFieldsEventArgs(
    IReadOnlyList<BlueprintVariableField> fields) : EventArgs
{
    public IReadOnlyList<BlueprintVariableField> Fields { get; } = fields;
}
