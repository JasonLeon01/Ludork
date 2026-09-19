using Avalonia.Controls;

namespace Ludork.Plugins.OfficialResourceCleanup.UI;

internal static class EditorInputs
{
    public static TextBox CreateEditableTextBox()
    {
        TextBox input = new() { Focusable = true, IsTabStop = true };
        input.Classes.Add("ludork-editable");
        return input;
    }
}
