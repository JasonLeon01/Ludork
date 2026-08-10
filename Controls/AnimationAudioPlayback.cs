using System;
using System.IO;

namespace Ludork.Controls;

internal interface IAnimationAudioPlayback : IDisposable
{
}

internal static class AnimationAudioPlayback
{
    public static IAnimationAudioPlayback? Create(string path, double offset)
    {
#if LUDORK_MACOS
        if (OperatingSystem.IsMacOS())
            return MacOSAnimationAudioPlayback.Create(path, offset);
        return null;
#elif LUDORK_WINDOWS
        if (OperatingSystem.IsWindows())
            return WindowsAnimationAudioPlayback.Create(path, offset);
        return null;
#else
        return null;
#endif
    }

    public static double? GetDuration(string path)
    {
        if (!File.Exists(path))
            return null;
#if LUDORK_MACOS
        if (OperatingSystem.IsMacOS())
            return MacOSAnimationAudioPlayback.GetDuration(path);
        return null;
#elif LUDORK_WINDOWS
        if (OperatingSystem.IsWindows())
            return WindowsAnimationAudioPlayback.GetDuration(path);
        return null;
#else
        return null;
#endif
    }
}
