using Ludork.Models;
using System;
using System.Threading;

namespace Ludork.Views.Utils;

internal sealed class EditorOperationProgressReporter : IProgress<EditorOperationProgress>
{
    private EditorOperationProgress current = new("EDIT_OPERATION_PREPARING");

    public EditorOperationProgress Current => Volatile.Read(ref current);

    public void Report(EditorOperationProgress value) => Volatile.Write(ref current, value);
}
