using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;

namespace Ludork.Views.Utils;

public static class EditorInputs
{
    public static Color EditableBackgroundColor => EditorTheme.Color("Input");
    public static Color ReadOnlyBackgroundColor => EditorTheme.Color("Surface");
    public static Color FieldBorderColor => EditorTheme.Color("Border");
    public static Color ReadOnlyBorderColor => FieldBorderColor;
    public const double FieldMinHeight = 28;
    public static readonly Thickness FieldPadding = new(8, 0);

    public const string EditableClass = "ludork-editable";
    public const string ReadOnlyClass = "ludork-readonly";

    public static TextBox CreateEditableTextBox(string? text = null)
    {
        TextBox box = new();
        ApplyEditable(box);
        if (text is not null)
            box.Text = text;
        return box;
    }

    public static TextBox CreateReadOnlyTextBox(string? text = null)
    {
        TextBox box = new();
        ApplyReadOnly(box);
        if (text is not null)
            box.Text = text;
        return box;
    }

    public static void ApplyEditable(TextBox box)
    {
        HintedTextPresenter.AttachTo(box);
        box.IsReadOnly = false;
        box.Focusable = true;
        box.ClearValue(TextBox.CaretBrushProperty);
        box.ClearValue(TextBox.CursorProperty);
        box.ClearValue(TextBox.ForegroundProperty);
        KeyboardNavigation.SetIsTabStop(box, true);
        box.Classes.Remove(ReadOnlyClass);
        if (!box.Classes.Contains(EditableClass))
            box.Classes.Add(EditableClass);
    }

    public static void ApplyReadOnly(TextBox box)
    {
        HintedTextPresenter.DetachFrom(box);
        box.IsReadOnly = true;
        box.Focusable = false;
        box.CaretBrush = Brushes.Transparent;
        if (Application.Current is not null)
            box.Cursor = new Cursor(StandardCursorType.Arrow);
        KeyboardNavigation.SetIsTabStop(box, false);
        box.Classes.Remove(EditableClass);
        if (!box.Classes.Contains(ReadOnlyClass))
            box.Classes.Add(ReadOnlyClass);
    }

    public static NumericUpDown CreateNumericUpDown(
        decimal value,
        decimal minimum,
        decimal maximum,
        decimal increment,
        bool stretch = true)
    {
        NumericUpDown box = new()
        {
            Value = value,
            Minimum = minimum,
            Maximum = maximum,
            Increment = increment,
        };
        ApplyEditable(box, stretch);
        return box;
    }

    public static void ApplyEditable(NumericUpDown box, bool stretch = true)
    {
        if (stretch)
            box.HorizontalAlignment = HorizontalAlignment.Stretch;
        if (!box.Classes.Contains(EditableClass))
            box.Classes.Add(EditableClass);
    }
}
