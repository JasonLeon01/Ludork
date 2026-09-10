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

    public bool matches(string query)
    {
        if (DisplayName.Contains(query, StringComparison.OrdinalIgnoreCase))
            return true;
        if (IsDirectory)
            return false;
        searchText ??= readSearchText();
        return searchText.Contains(query, StringComparison.OrdinalIgnoreCase);
    }

    private string readSearchText()
    {
        return File.ReadAllText(Path, Encoding.UTF8);
    }
}
