using NAudio.Vorbis;
using NAudio.Wave;
using NAudio.Wave.SampleProviders;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Controls;

[SupportedOSPlatform("macos")]
internal sealed class MacOSAnimationAudioPlayback : IAnimationAudioPlayback
{
    private readonly nint sound;

    private MacOSAnimationAudioPlayback(nint sound)
    {
        this.sound = sound;
    }

    public static MacOSAnimationAudioPlayback? Create(string path, double offset)
    {
        nint sound = createSound(path);
        if (sound == nint.Zero)
            return null;
        MacOSSound.SetCurrentTime(sound, Math.Max(0, offset));
        if (!MacOSSound.Play(sound))
        {
            MacOSSound.Release(sound);
            return null;
        }
        return new MacOSAnimationAudioPlayback(sound);
    }

    public static double? GetDuration(string path)
    {
        if (Path.GetExtension(path).Equals(".ogg", StringComparison.OrdinalIgnoreCase))
        {
            using VorbisWaveReader vorbisReader = new(path);
            return vorbisReader.TotalTime.TotalSeconds;
        }
        nint sound = MacOSSound.Create(path);
        if (sound == nint.Zero)
            return null;
        double duration = MacOSSound.GetDuration(sound);
        MacOSSound.Release(sound);
        return duration;
    }

    private static nint createSound(string path)
    {
        if (!Path.GetExtension(path).Equals(".ogg", StringComparison.OrdinalIgnoreCase))
            return MacOSSound.Create(path);
        string temporaryPath = Path.Combine(
            Path.GetTempPath(),
            $"ludork-audio-{Guid.NewGuid():N}.wav"
        );
        try
        {
            using VorbisWaveReader reader = new(path);
            ISampleProvider sampleProvider = reader.ToSampleProvider();
            IWaveProvider waveProvider = new SampleToWaveProvider16(sampleProvider);
            WaveFileWriter.CreateWaveFile(temporaryPath, waveProvider);
            return MacOSSound.Create(temporaryPath);
        }
        finally
        {
            File.Delete(temporaryPath);
        }
    }

    public void Dispose()
    {
        MacOSSound.Stop(sound);
        MacOSSound.Release(sound);
    }
}

[SupportedOSPlatform("macos")]
internal static class MacOSSound
{
    private const string ObjectiveCLibrary = "/usr/lib/libobjc.A.dylib";
    private static readonly nint allocSelector = sel_registerName("alloc");
    private static readonly nint initWithUtf8Selector = sel_registerName("initWithUTF8String:");
    private static readonly nint initWithFileSelector = sel_registerName("initWithContentsOfFile:byReference:");
    private static readonly nint setCurrentTimeSelector = sel_registerName("setCurrentTime:");
    private static readonly nint durationSelector = sel_registerName("duration");
    private static readonly nint playSelector = sel_registerName("play");
    private static readonly nint stopSelector = sel_registerName("stop");
    private static readonly nint releaseSelector = sel_registerName("release");

    public static nint Create(string path)
    {
        nint stringClass = objc_getClass("NSString");
        nint pathString = sendIntPtr(sendIntPtr(stringClass, allocSelector), initWithUtf8Selector, path);
        nint soundClass = objc_getClass("NSSound");
        nint sound = sendIntPtrBool(sendIntPtr(soundClass, allocSelector), initWithFileSelector, pathString, false);
        sendVoid(pathString, releaseSelector);
        return sound;
    }

    public static void SetCurrentTime(nint sound, double time)
    {
        sendVoidDouble(sound, setCurrentTimeSelector, time);
    }

    public static double GetDuration(nint sound)
    {
        return sendDouble(sound, durationSelector);
    }

    public static bool Play(nint sound)
    {
        return sendBool(sound, playSelector);
    }

    public static void Stop(nint sound)
    {
        sendVoid(sound, stopSelector);
    }

    public static void Release(nint sound)
    {
        sendVoid(sound, releaseSelector);
    }

    [DllImport(ObjectiveCLibrary)]
    private static extern nint objc_getClass(string name);

    [DllImport(ObjectiveCLibrary)]
    private static extern nint sel_registerName(string name);

    [DllImport(ObjectiveCLibrary, EntryPoint = "objc_msgSend")]
    private static extern nint sendIntPtr(nint receiver, nint selector);

    [DllImport(ObjectiveCLibrary, EntryPoint = "objc_msgSend")]
    private static extern nint sendIntPtr(nint receiver, nint selector, string value);

    [DllImport(ObjectiveCLibrary, EntryPoint = "objc_msgSend")]
    private static extern nint sendIntPtrBool(nint receiver, nint selector, nint value, [MarshalAs(UnmanagedType.I1)] bool flag);

    [DllImport(ObjectiveCLibrary, EntryPoint = "objc_msgSend")]
    private static extern void sendVoid(nint receiver, nint selector);

    [DllImport(ObjectiveCLibrary, EntryPoint = "objc_msgSend")]
    private static extern void sendVoidDouble(nint receiver, nint selector, double value);

    [DllImport(ObjectiveCLibrary, EntryPoint = "objc_msgSend")]
    private static extern double sendDouble(nint receiver, nint selector);

    [return: MarshalAs(UnmanagedType.I1)]
    [DllImport(ObjectiveCLibrary, EntryPoint = "objc_msgSend")]
    private static extern bool sendBool(nint receiver, nint selector);
}
