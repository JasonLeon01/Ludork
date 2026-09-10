using System;

namespace Ludork.Controls;

public sealed class UiAnimationKeyMoveEventArgs : EventArgs
{
    public UiAnimationKeyMoveEventArgs(string track, int keyIndex, double time)
    {
        Track = track;
        KeyIndex = keyIndex;
        Time = time;
    }

    public string Track { get; }
    public int KeyIndex { get; }
    public double Time { get; }
}
