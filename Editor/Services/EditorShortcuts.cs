using Avalonia.Input;
using System;

namespace Ludork.Services;

public static class EditorShortcuts
{
    public static KeyModifiers PrimaryModifier => OperatingSystem.IsMacOS()
        ? KeyModifiers.Meta
        : KeyModifiers.Control;

    public static bool HasPrimaryModifier(KeyModifiers modifiers)
    {
        return modifiers.HasFlag(PrimaryModifier);
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
