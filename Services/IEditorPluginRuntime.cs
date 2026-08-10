using Ludork.Plugin.Abstractions;
using System.Threading.Tasks;

namespace Ludork.Services;

public interface IEditorPluginRuntime
{
    string? ResolveTextHint(TextHintContext context);
    Task<PluginResult> ExecuteBeforeProjectOperationAsync(ProjectOperationContext context);
}
