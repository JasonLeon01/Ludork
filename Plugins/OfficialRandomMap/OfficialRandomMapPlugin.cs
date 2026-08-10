using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialRandomMap.Localization;
using Ludork.Plugins.OfficialRandomMap.UI;
using System;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialRandomMap;

public sealed class OfficialRandomMapPlugin : IEditorPlugin
{
    public void Register(IPluginRegistrar registrar)
    {
        ArgumentNullException.ThrowIfNull(registrar);
        PluginLocalizer localizer = PluginLocalizer.Load(
            registrar.PluginDirectory,
            registrar.EditorLanguage);
        PluginMapContextMenuCommand command = new(
            "Ludork.OfficialRandomMap.Open",
            100,
            localizer.Text("menuLabel"),
            context => OpenAsync(context, localizer));
        registrar.RegisterMapContextMenuCommand(command);
    }

    private static async Task<PluginResult> OpenAsync(
        PluginMapContextMenuContext context,
        PluginLocalizer localizer)
    {
        if (context.UserInterface is not IAvaloniaPluginUserInterface avaloniaUi)
        {
            string error = localizer.Text("requiresAvalonia");
            await context.UserInterface.ShowMessageAsync(
                localizer.Text("operationFailed"),
                error,
                PluginMessageKind.Error,
                context.CancellationToken);
            return PluginResult.Failed(error);
        }

        RandomMapWindow window = new(
            context.MapEditorHost,
            context.MapKey,
            localizer,
            context.CancellationToken);
        await window.ShowDialog(avaloniaUi.Owner);
        return PluginResult.Completed();
    }
}
