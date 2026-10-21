using Avalonia.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorResourceOperations
{
    public static Task<bool> DeleteAsync(Window owner, ProjectDataStore store,
        Func<IProgress<EditorOperationProgress>, Task<bool>> operation)
        => EditorEditWorkflow.RunAsync(owner, store, LocaleService.Get("DELETE"), operation, false);
}
