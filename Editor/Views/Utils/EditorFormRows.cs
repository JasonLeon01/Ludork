using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;

namespace Ludork.Views.Utils;

internal static class EditorFormRows
{
    public static Grid Create(string label, Control editor)
    {
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("160,*"),
            ColumnSpacing = 12,
        };
        row.Children.Add(new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
        });
        Grid.SetColumn(editor, 1);
        row.Children.Add(editor);
        return row;
    }

    public static void Add(Grid form, string label, Control editor)
    {
        int rowIndex = form.RowDefinitions.Count;
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid row = Create(label, editor);
        Grid.SetRow(row, rowIndex);
        form.Children.Add(row);
    }
}
