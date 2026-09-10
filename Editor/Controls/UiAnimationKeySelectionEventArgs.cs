using System;

namespace Ludork.Controls;

public sealed class UiAnimationKeySelectionEventArgs : EventArgs
{
    public UiAnimationKeySelectionEventArgs(string track, int keyIndex)
    {
        Track = track;
        KeyIndex = keyIndex;
    }

    public string Track { get; }
    public int KeyIndex { get; }
}
