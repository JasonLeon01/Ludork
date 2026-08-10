using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialBlueprintAI.Localization;
using Ludork.Plugins.OfficialBlueprintAI.UI;

namespace Ludork.Plugins.OfficialBlueprintAI;

public sealed class OfficialBlueprintAIPlugin : IEditorPlugin
{
    private readonly Dictionary<string, BlueprintAssistantWindow> windows =
        new Dictionary<string, BlueprintAssistantWindow>(
            OperatingSystem.IsWindows()
                ? StringComparer.OrdinalIgnoreCase
                : StringComparer.Ordinal);

    public void Register(IPluginRegistrar registrar)
    {
        ArgumentNullException.ThrowIfNull(registrar);
        PluginLocalizer localizer = PluginLocalizer.Load(
            registrar.PluginDirectory,
            registrar.EditorLanguage);
        PluginUiText.Initialize(localizer);
        BlueprintAssistantProvider provider = new BlueprintAssistantProvider(
            localizer);
        PluginMenuCommand command = new PluginMenuCommand(
            "Ludork.OfficialBlueprintAI.Open",
            PluginMenuLocation.Game,
            100,
            localizer.Text("menuLabel"),
            context => OpenAsync(context, provider, localizer));
        registrar.RegisterMenuCommand(command);
    }

    private async Task<PluginResult> OpenAsync(
        PluginMenuContext context,
        IBlueprintAssistantProvider provider,
        PluginLocalizer localizer)
    {
        if (context.BlueprintAssistantHost is null)
        {
            string message = localizer.Text("missingProject");
            await context.UserInterface.ShowMessageAsync(
                localizer.Text("assistantTitle"),
                message,
                PluginMessageKind.Warning,
                context.CancellationToken);
            return PluginResult.Failed(message);
        }

        string projectPath = NormalizeProjectPath(
            context.BlueprintAssistantHost.ProjectPath);
        if (windows.TryGetValue(
            projectPath,
            out BlueprintAssistantWindow? existing))
        {
            existing.Activate();
            return PluginResult.Completed();
        }

        BlueprintAssistantProviderContext providerContext =
            new BlueprintAssistantProviderContext(
                "Ludork.OfficialBlueprintAI",
                context.PluginDirectory,
                context.PluginDataDirectory,
                context.EditorLanguage,
                context.SecretStore);
        BlueprintAssistantWindow window = new BlueprintAssistantWindow(
            provider,
            providerContext,
            context.BlueprintAssistantHost,
            localizer.Text("assistantTitle"));
        windows[projectPath] = window;
        window.Closed += (_, _) =>
        {
            if (windows.TryGetValue(
                    projectPath,
                    out BlueprintAssistantWindow? current) &&
                ReferenceEquals(current, window))
            {
                windows.Remove(projectPath);
            }
        };
        if (context.UserInterface is IAvaloniaPluginUserInterface avaloniaUi)
        {
            window.Show(avaloniaUi.Owner);
        }
        else
        {
            window.Show();
        }
        return PluginResult.Completed();
    }

    private static string NormalizeProjectPath(string projectPath)
    {
        return Path.GetFullPath(projectPath).TrimEnd(
            Path.DirectorySeparatorChar,
            Path.AltDirectorySeparatorChar);
    }
}
