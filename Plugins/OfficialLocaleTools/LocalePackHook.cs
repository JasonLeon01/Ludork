using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialLocaleTools;

internal sealed class LocalePackHook : IProjectOperationHook
{
    public Task<PluginResult> ExecuteAsync(ProjectOperationContext context)
    {
        context.CancellationToken.ThrowIfCancellationRequested();
        context.Packaging?.ExcludeFile(LocaleProjectPaths.WorkbookRelativePath);
        return Task.FromResult(PluginResult.Completed());
    }
}
