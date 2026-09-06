using System;

namespace Ludork.Services.Plugins;

internal sealed class PluginCompilationException : Exception
{
    public PluginCompilationException(string diagnostics)
        : base(string.IsNullOrWhiteSpace(diagnostics)
            ? "Plugin compilation failed."
            : diagnostics)
    {
    }
}
