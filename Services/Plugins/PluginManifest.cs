using System.Text.Json.Serialization;

namespace Ludork.Services.Plugins;

internal sealed class PluginManifest
{
    [JsonPropertyName("schemaVersion")]
    public int SchemaVersion { get; set; }

    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("version")]
    public string Version { get; set; } = string.Empty;

    [JsonPropertyName("minimumEditorVersion")]
    public string MinimumEditorVersion { get; set; } = string.Empty;

    [JsonPropertyName("entryType")]
    public string EntryType { get; set; } = string.Empty;
}
