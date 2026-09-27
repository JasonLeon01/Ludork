using Avalonia.Controls;
using System;
using System.Linq;

namespace Ludork.Services;

internal static class EditorPanelLayoutBinding
{
    public static void Bind(Grid grid, GridSplitter splitter, string key, bool rows, int[] indices, EditorSettings settings)
    {
        GridLength[]? saved = settings.GetPanelLayout(key);
        if (saved is not null && saved.Length == indices.Length)
        {
            for (int index = 0; index < indices.Length; index++)
            {
                if (rows)
                    grid.RowDefinitions[indices[index]].Height = saved[index];
                else
                    grid.ColumnDefinitions[indices[index]].Width = saved[index];
            }
        }
        EditorSplitterChangeBinding.Attach(splitter,
            () => indices.Select(index => rows
                ? grid.RowDefinitions[index].Height : grid.ColumnDefinitions[index].Width).ToArray(),
            lengths =>
            {
                if (lengths.Any(length => !length.IsAuto && (!double.IsFinite(length.Value) || length.Value < 0)))
                    return;
                settings.SetPanelLayout(key, lengths);
                EditorLayoutService.Save();
            });
    }
}
