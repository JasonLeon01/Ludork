using System.Collections.Generic;

namespace Ludork.Views;

public sealed class MarkdownDocumentSection
{
    public MarkdownDocumentSection(
        string displayName,
        MarkdownDocumentEntry rootEntry,
        IReadOnlyList<MarkdownDocumentEntry> treeEntries)
    {
        DisplayName = displayName;
        RootEntry = rootEntry;
        TreeEntries = treeEntries;
    }

    public string DisplayName { get; }
    public MarkdownDocumentEntry RootEntry { get; }
    public IReadOnlyList<MarkdownDocumentEntry> TreeEntries { get; }
    public MarkdownDocumentEntry? SelectedEntry { get; set; }
}
