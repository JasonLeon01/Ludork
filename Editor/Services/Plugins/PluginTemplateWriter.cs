using System;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.CodeAnalysis.CSharp;

namespace Ludork.Services.Plugins;

internal static class PluginTemplateWriter
{
    public static async Task WriteAsync(
        string directory,
        PluginCreationRequest request,
        CancellationToken cancellationToken)
    {
        Version version = typeof(PluginTemplateWriter).Assembly.GetName().Version ?? new Version(1, 0, 0);
        string manifestTemplate = await readTemplateAsync("Common.plugin.json", cancellationToken);
        PluginManifest manifest = JsonSerializer.Deserialize<PluginManifest>(manifestTemplate)
            ?? throw new InvalidDataException("Plugin manifest template is empty.");
        manifest.Id = request.Id;
        manifest.Name = request.Name;
        manifest.MinimumEditorVersion = version.ToString(3);
        Directory.CreateDirectory(directory);
        await File.WriteAllTextAsync(
            Path.Combine(directory, "plugin.json"),
            JsonSerializer.Serialize(manifest, new JsonSerializerOptions { WriteIndented = true }) + Environment.NewLine,
            cancellationToken);
        await File.WriteAllTextAsync(
            Path.Combine(directory, "Plugin.cs"),
            await readTemplateAsync("Common.Plugin.cs.template", cancellationToken),
            cancellationToken);
        if (!request.WithWindow)
            return;
        string windowTemplate = await readTemplateAsync("Window.PluginWindow.cs.template", cancellationToken);
        await File.WriteAllTextAsync(
            Path.Combine(directory, "PluginWindow.cs"),
            windowTemplate.Replace("{{PluginTitle}}", SymbolDisplay.FormatLiteral(request.Name, true), StringComparison.Ordinal),
            cancellationToken);
    }

    private static async Task<string> readTemplateAsync(string name, CancellationToken cancellationToken)
    {
        using Stream stream = typeof(PluginTemplateWriter).Assembly.GetManifestResourceStream(
            "Ludork.Editor.PluginTemplates." + name)
            ?? throw new FileNotFoundException($"Plugin template resource was not found: {name}");
        using StreamReader reader = new(stream);
        return await reader.ReadToEndAsync(cancellationToken);
    }
}
