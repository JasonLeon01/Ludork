using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;

namespace Ludork.Views.Utils;

public static class EditorInputs
{
    public static readonly Color EditableBackgroundColor = Color.Parse("#333333");
    public static readonly Color ReadOnlyBackgroundColor = Color.Parse("#262626");
    public static readonly Color FieldBorderColor = Color.Parse("#464646");
    public static readonly Color ReadOnlyBorderColor = FieldBorderColor;
    public const double FieldMinHeight = 34;
    public static readonly Thickness FieldPadding = new(16, 0);

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
        box.Height = FieldMinHeight;
        box.MinHeight = FieldMinHeight;
        box.Padding = FieldPadding;
        box.Background = new SolidColorBrush(EditableBackgroundColor);
        box.BorderBrush = new SolidColorBrush(FieldBorderColor);
        box.BorderThickness = new Thickness(1);
        box.CornerRadius = new CornerRadius(4);
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
        box.Background = new SolidColorBrush(ReadOnlyBackgroundColor);
        box.BorderBrush = new SolidColorBrush(FieldBorderColor);
        box.BorderThickness = new Thickness(1);
        box.CornerRadius = new CornerRadius(4);
        box.Padding = FieldPadding;
        box.Height = FieldMinHeight;
        box.MinHeight = FieldMinHeight;
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
        box.Height = FieldMinHeight;
        box.MinHeight = FieldMinHeight;
        box.Padding = FieldPadding;
        box.Background = new SolidColorBrush(EditableBackgroundColor);
        box.BorderBrush = new SolidColorBrush(FieldBorderColor);
        box.BorderThickness = new Thickness(1);
        box.CornerRadius = new CornerRadius(4);
        if (stretch)
            box.HorizontalAlignment = HorizontalAlignment.Stretch;
        if (!box.Classes.Contains(EditableClass))
            box.Classes.Add(EditableClass);
    }
}
