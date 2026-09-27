using Avalonia.Controls;
using Avalonia.Threading;
using System;
using System.Runtime.CompilerServices;

namespace Ludork.Services;

public static class EditorLayoutService
{
    private static readonly ConditionalWeakTable<Window, EditorWindowStateBinding> windows = new();
    private static readonly DispatcherTimer saveTimer = new() { Interval = TimeSpan.FromMilliseconds(300) };

    static EditorLayoutService()
    {
        saveTimer.Tick += (_, _) => Save();
    }

    public static EditorSettings? Settings { get; private set; }

    public static void Initialize(EditorSettings settings)
    {
        saveTimer.Stop();
        Settings = settings;
    }

    public static void RequestSave()
    {
        saveTimer.Stop();
        saveTimer.Start();
    }

    public static void Save()
    {
        saveTimer.Stop();
        Settings?.Save();
    }

    public static void AttachWindow(Window window, string key)
    {
        if (Settings is not null && !windows.TryGetValue(window, out _))
            windows.Add(window, new EditorWindowStateBinding(window, key, Settings));
    }

    public static void BindColumns(Grid grid, GridSplitter splitter, string key, params int[] indices)
    {
        if (Settings is not null)
            EditorPanelLayoutBinding.Bind(grid, splitter, key, false, indices, Settings);
    }

    public static void BindRows(Grid grid, GridSplitter splitter, string key, params int[] indices)
    {
        if (Settings is not null)
            EditorPanelLayoutBinding.Bind(grid, splitter, key, true, indices, Settings);
    }

    public static void BindExpander(Expander expander, string key)
    {
        if (Settings is not EditorSettings settings)
            return;
        expander.IsExpanded = settings.GetExpanded(key, expander.IsExpanded);
        expander.PropertyChanged += (_, args) =>
        {
            if (args.Property != Expander.IsExpandedProperty)
                return;
            settings.SetExpanded(key, expander.IsExpanded);
            Save();
        };
    }
}
