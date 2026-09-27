using Ludork.Services;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Text;

namespace Ludork.Views;

public sealed class MarkdownDocumentEntry : INotifyPropertyChanged
{
    private bool isVisible = true;
    private string? searchText;

    public MarkdownDocumentEntry(string displayName, string path, bool isDirectory)
    {
        DisplayName = displayName;
        Path = path;
        IsDirectory = isDirectory;
    }

    public string DisplayName { get; }
    public string Path { get; }
    public bool IsDirectory { get; }
    public ObservableCollection<MarkdownDocumentEntry> Children { get; } = [];
    public bool IsVisible
    {
        get => isVisible;
        set
        {
            if (isVisible == value)
                return;
            isVisible = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsVisible)));
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public bool matches(string query, string root)
    {
        if (DisplayName.Contains(query, StringComparison.OrdinalIgnoreCase))
            return true;
        if (IsDirectory)
            return false;
        searchText ??= TryReadText(root, out string content) ? content : string.Empty;
        return searchText.Contains(query, StringComparison.OrdinalIgnoreCase);
    }

    public bool TryReadText(string root, out string content)
    {
        content = string.Empty;
        if (IsDirectory || !EditorPathSandbox.TryResolve(root, Path, out string path))
            return false;
        try
        {
            content = File.ReadAllText(path, Encoding.UTF8);
            return true;
        }
        catch (Exception exception) when (EditorPathSandbox.IsPathFailure(exception))
        {
            return false;
        }
    }
}
