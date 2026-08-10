using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialLocaleTools;

public sealed class OfficialLocaleToolsPlugin : IEditorPlugin
{
    public void Register(IPluginRegistrar registrar)
    {
        ArgumentNullException.ThrowIfNull(registrar);

        string localePath = Path.Combine(registrar.PluginDirectory, "locale.json");
        PluginLocalizer localizer = PluginLocalizer.Load(localePath, registrar.EditorLanguage);
        LocaleTextHintProvider textHintProvider = new LocaleTextHintProvider();
        LocaleExportHook exportHook = new LocaleExportHook(localizer, textHintProvider);

        PluginMenuCommand openWorkbookCommand = new PluginMenuCommand(
            "Ludork.OfficialLocaleTools.OpenWorkbook",
            PluginMenuLocation.Database,
            900,
            localizer.Text("MENU_OPEN_LOCALE_TABLE"),
            context => OpenWorkbookAsync(context, localizer));

        registrar.RegisterMenuCommand(openWorkbookCommand);
        registrar.RegisterTextHintProvider(textHintProvider);
        registrar.RegisterBeforeRunHook(exportHook);
        registrar.RegisterBeforePackHook(exportHook);
    }

    private static async Task<PluginResult> OpenWorkbookAsync(
        PluginMenuContext context,
        PluginLocalizer localizer)
    {
        context.CancellationToken.ThrowIfCancellationRequested();
        if (string.IsNullOrWhiteSpace(context.ProjectPath))
        {
            string projectError = localizer.Text("PROJECT_NOT_OPEN");
            await context.UserInterface.ShowMessageAsync(
                localizer.Text("LOCALE_TOOLS_TITLE"),
                projectError,
                PluginMessageKind.Warning,
                context.CancellationToken);
            return PluginResult.Failed(projectError);
        }

        string workbookPath = LocaleProjectPaths.GetWorkbookPath(context.ProjectPath);
        if (!File.Exists(workbookPath))
        {
            string missingError = localizer.Format("LOCALE_WORKBOOK_NOT_FOUND", workbookPath);
            await context.UserInterface.ShowMessageAsync(
                localizer.Text("LOCALE_TOOLS_TITLE"),
                missingError,
                PluginMessageKind.Warning,
                context.CancellationToken);
            return PluginResult.Failed(missingError);
        }

        string? openError = TryOpenWorkbook(workbookPath, localizer);
        if (openError is not null)
        {
            await context.UserInterface.ShowMessageAsync(
                localizer.Text("LOCALE_TOOLS_TITLE"),
                openError,
                PluginMessageKind.Error,
                context.CancellationToken);
            return PluginResult.Failed(openError);
        }

        return PluginResult.Completed();
    }

    private static string? TryOpenWorkbook(string workbookPath, PluginLocalizer localizer)
    {
        try
        {
            ProcessStartInfo startInfo;
            if (OperatingSystem.IsWindows())
            {
                startInfo = new ProcessStartInfo(workbookPath)
                {
                    UseShellExecute = true,
                };
            }
            else
            {
                string launcher = OperatingSystem.IsMacOS() ? "open" : "xdg-open";
                startInfo = new ProcessStartInfo(launcher)
                {
                    UseShellExecute = false,
                };
                startInfo.ArgumentList.Add(workbookPath);
            }

            using Process? process = Process.Start(startInfo);
            if (process is null)
            {
                throw new InvalidOperationException("The operating system did not start an application.");
            }
            return null;
        }
        catch (Exception exception)
        {
            return localizer.Format("OPEN_LOCALE_WORKBOOK_FAILED", exception.Message);
        }
    }
}
