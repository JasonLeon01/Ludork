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
}
