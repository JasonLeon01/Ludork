using System.Collections.Generic;
using System.Text.Json.Serialization;

namespace Ludork.Services.Plugins;

internal sealed class PluginRegistryDocument
{
    [JsonPropertyName("schemaVersion")]
    public int SchemaVersion { get; set; } = 1;

    [JsonPropertyName("plugins")]
    public List<PluginRegistryEntry> Plugins { get; set; } = [];

    [JsonPropertyName("pendingDelete")]
    public List<string> PendingDelete { get; set; } = [];

    public PluginRegistryDocument Clone()
    {
        PluginRegistryDocument document = new()
        {
            SchemaVersion = SchemaVersion,
        };
        foreach (PluginRegistryEntry entry in Plugins)
            document.Plugins.Add(new PluginRegistryEntry { Id = entry.Id, Directory = entry.Directory });
        document.PendingDelete.AddRange(PendingDelete);
        return document;
    }
}
