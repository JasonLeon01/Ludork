using Avalonia.Controls;

namespace Ludork.Views;

public sealed class MarkdownHeading
{
    public MarkdownHeading(string text, int level, string anchor)
    {
        Text = text;
        Level = level;
        Anchor = anchor;
    }

    public string Text { get; }
    public int Level { get; }
    public string Anchor { get; }
    public Button Button { get; set; } = null!;
    public bool IsCollapsed { get; set; }
}
