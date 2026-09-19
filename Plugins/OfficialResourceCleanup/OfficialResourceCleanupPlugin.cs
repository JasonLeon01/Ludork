using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialResourceCleanup.Localization;
using Ludork.Plugins.OfficialResourceCleanup.UI;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialResourceCleanup;

public sealed class OfficialResourceCleanupPlugin : IEditorPlugin
{
    public void Register(IPluginRegistrar registrar)
    {
        ArgumentNullException.ThrowIfNull(registrar);
        PluginLocalizer localizer = PluginLocalizer.Load(
            registrar.PluginDirectory, registrar.EditorLanguage);
        registrar.RegisterMenuCommand(new PluginMenuCommand(
            "Ludork.OfficialResourceCleanup.Open",
            PluginMenuLocation.Plugins,
            200,
            localizer.Text("menuLabel"),
            context => openAsync(context, localizer)));
    }

    private static async Task<PluginResult> openAsync(
        PluginMenuContext context,
        PluginLocalizer localizer)
    {
        if (context.ResourceCleanupHost is not IResourceCleanupHost host
            || context.UserInterface is not IAvaloniaPluginUserInterface avaloniaUi)
        {
            string error = localizer.Text("missingProject");
            await context.UserInterface.ShowMessageAsync(
                localizer.Text("windowTitle"), error,
                PluginMessageKind.Warning, context.CancellationToken);
            return PluginResult.Failed(error);
        }
        string[] nativePaths = (await File.ReadAllLinesAsync(
                Path.Combine(context.PluginDirectory, "NativeResources.list"),
                context.CancellationToken))
            .Select(line => line.Trim())
            .Where(line => line.Length != 0 && !line.StartsWith('#'))
            .Distinct(StringComparer.Ordinal)
            .ToArray();
        ResourceCleanupWindow window = new(host, nativePaths, localizer,
            context.CancellationToken);
        await window.ShowDialog(avaloniaUi.Owner);
        return PluginResult.Completed();
    }
}
