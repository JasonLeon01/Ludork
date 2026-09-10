using Ludork.Plugin.Abstractions;
using System;
using System.IO;

namespace Ludork.Services;

public static class TextHintService
{
    private static readonly TextHintRefresh refresh = new();
    private static IEditorPluginRuntime? runtime;
    private static string projectPath = string.Empty;

    static TextHintService()
    {
        LocaleService.LanguageChanged += (_, _) => Invalidate();
    }

    public static event EventHandler? Invalidated;

    public static IEditorPluginRuntime? Runtime => runtime;
    public static ITextHintRefresh Refresh => refresh;

    public static void Configure(IEditorPluginRuntime? pluginRuntime, string? activeProjectPath = null)
    {
        runtime = pluginRuntime;
        SetProjectPath(activeProjectPath);
    }

    public static void SetProjectPath(string? activeProjectPath)
    {
        projectPath = string.IsNullOrWhiteSpace(activeProjectPath)
            ? string.Empty
            : Path.GetFullPath(activeProjectPath);
        Invalidate();
    }

    public static string? Resolve(string? text)
    {
        if (runtime is null || projectPath.Length == 0 || string.IsNullOrWhiteSpace(text))
            return null;
        string token = text.Trim();
        if (!isSingleToken(token))
            return null;
        return runtime.ResolveTextHint(new TextHintContext(
            projectPath,
            LocaleService.CurrentLanguage,
            token));
    }

    public static void Invalidate()
    {
        Invalidated?.Invoke(null, EventArgs.Empty);
    }

    private static bool isSingleToken(string token)
    {
        if (token.Length < 3 || token[0] != '{' || token[^1] != '}')
            return false;
        for (int index = 1; index < token.Length - 1; index++)
        {
            if (token[index] is '{' or '}')
                return false;
        }
        return true;
    }

    private sealed class TextHintRefresh : ITextHintRefresh
    {
        public void Invalidate()
        {
            TextHintService.Invalidate();
        }
    }
}
