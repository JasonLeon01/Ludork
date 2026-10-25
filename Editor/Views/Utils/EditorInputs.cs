using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Ludork.Services;
using SharedInputs = Ludork.Plugin.Avalonia.EditorInputs;

namespace Ludork.Views.Utils;

public static class EditorInputs
{
    public static Color EditableBackgroundColor => SharedInputs.EditableBackgroundColor;
    public static Color ReadOnlyBackgroundColor => SharedInputs.ReadOnlyBackgroundColor;
    public static Color FieldBorderColor => SharedInputs.FieldBorderColor;
    public static Color ReadOnlyBorderColor => FieldBorderColor;
    public const double FieldMinHeight = SharedInputs.FieldMinHeight;
    public static readonly Thickness FieldPadding = SharedInputs.FieldPadding;

    public const string EditableClass = SharedInputs.EditableClass;
    public const string ReadOnlyClass = SharedInputs.ReadOnlyClass;

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
        SharedInputs.ApplyEditable(box);
    }

    public static void ApplyReadOnly(TextBox box)
    {
        HintedTextPresenter.DetachFrom(box);
        SharedInputs.ApplyReadOnly(box);
    }

    public static NumericUpDown CreateNumericUpDown(
        decimal value,
        decimal minimum,
        decimal maximum,
        decimal increment,
        bool stretch = true)
    {
        return SharedInputs.CreateNumericUpDown(value, minimum, maximum, increment, stretch);
    }

    public static void ApplyEditable(NumericUpDown box, bool stretch = true)
    {
        SharedInputs.ApplyEditable(box, stretch);
    }
}
