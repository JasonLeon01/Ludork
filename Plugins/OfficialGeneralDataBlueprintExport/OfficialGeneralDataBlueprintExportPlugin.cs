using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialGeneralDataBlueprintExport.UI;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport;

public sealed class OfficialGeneralDataBlueprintExportPlugin : IEditorPlugin
{
    public void Register(IPluginRegistrar registrar)
    {
        PluginLocalizer localizer = PluginLocalizer.LoadDirectory(registrar.PluginDirectory, registrar.EditorLanguage);
        registrar.RegisterMenuCommand(new PluginMenuCommand(
            "Ludork.OfficialGeneralDataBlueprintExport.Open", PluginMenuLocation.Plugins, 210,
            localizer.Text("menuLabel"), context => openAsync(context, localizer)));
    }

    private static async Task<PluginResult> openAsync(PluginMenuContext context, PluginLocalizer localizer)
    {
        if (context.BlueprintWorkspace is not IPluginBlueprintWorkspace workspace
            || context.UserInterface is not IAvaloniaPluginUserInterface ui)
            return PluginResult.Failed(localizer.Text("missingProject"));

        GeneralDataBlueprintExportWindow window = new(new GeneralDataBlueprintExporter(workspace), localizer, context.CancellationToken);
        await window.ShowDialog(ui.Owner);
        return PluginResult.Completed();
    }
}
