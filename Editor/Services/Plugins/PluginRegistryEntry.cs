using System.Text.Json.Serialization;

namespace Ludork.Services.Plugins;

internal sealed class PluginRegistryEntry
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    [JsonPropertyName("directory")]
    public string Directory { get; set; } = string.Empty;
}
