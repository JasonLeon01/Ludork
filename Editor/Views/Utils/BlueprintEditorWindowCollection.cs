using Avalonia.Controls;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views.Utils;

internal sealed class BlueprintEditorWindowCollection
{
    private readonly Dictionary<string, BlueprintEditorWindow> windows = new(StringComparer.Ordinal);

    public BlueprintEditorWindow[] Windows => windows.Values.Distinct().ToArray();

    public BlueprintEditorWindow? Find(string documentKey) => windows.GetValueOrDefault(documentKey);

    public void Reindex()
    {
        BlueprintEditorWindow[] current = Windows;
        windows.Clear();
        foreach (BlueprintEditorWindow window in current)
            windows[window.Document.DocumentKey] = window;
    }

    public void Open(
        Window owner,
        BlueprintEditorDocument? document,
        Func<BlueprintEditorDocument, BlueprintEditorWindow> createWindow)
    {
        if (document is null)
            return;
        if (Find(document.DocumentKey) is BlueprintEditorWindow existing)
        {
            document.Dispose();
            if (!existing.Reload())
                return;
            existing.Show();
            existing.Activate();
            return;
        }
        BlueprintEditorWindow window = createWindow(document);
        windows[document.DocumentKey] = window;
        window.Closed += (_, _) => windows.Remove(window.Document.DocumentKey);
        window.Show(owner);
    }
}
