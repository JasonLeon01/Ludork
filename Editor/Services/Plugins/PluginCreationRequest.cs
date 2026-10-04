namespace Ludork.Services.Plugins;

public sealed record PluginCreationRequest(string Id, string Name, bool WithWindow);
