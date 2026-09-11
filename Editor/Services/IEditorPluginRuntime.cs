using Ludork.Plugin.Abstractions;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Ludork.Services;

public interface IEditorPluginRuntime
{
    string? ResolveTextHint(TextHintContext context);
    IReadOnlyList<ProjectExportParticipant> GetExportParticipants(string projectPath);
    Task<PluginResult> ExecuteBeforeProjectOperationAsync(ProjectOperationContext context);
}
