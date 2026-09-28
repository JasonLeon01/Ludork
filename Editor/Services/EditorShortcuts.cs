using Avalonia.Input;
using Ludork.Plugin.Avalonia;

namespace Ludork.Services;

public static class EditorShortcuts
{
    public static KeyModifiers PrimaryModifier => EditorZoomInput.PrimaryModifier;

    public static bool HasPrimaryModifier(KeyModifiers modifiers)
    {
        return EditorZoomInput.HasPrimaryModifier(modifiers);
    }

    public static bool IsUndo(Key key, KeyModifiers modifiers)
    {
        return key == Key.Z && modifiers == PrimaryModifier;
    }

    public static bool IsRedo(Key key, KeyModifiers modifiers)
    {
        return key == Key.Y && modifiers == PrimaryModifier
            || key == Key.Z && modifiers == (PrimaryModifier | KeyModifiers.Shift);
    }
}
