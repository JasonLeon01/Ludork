using NAudio.Vorbis;
using NAudio.Wave;
using System;
using System.IO;
using System.Runtime.Versioning;

namespace Ludork.Controls;

[SupportedOSPlatform("windows")]
internal sealed class WindowsAnimationAudioPlayback : IAnimationAudioPlayback
{
    private readonly IWavePlayer output;
    private readonly WaveStream reader;

    private WindowsAnimationAudioPlayback(WaveStream reader)
    {
        this.reader = reader;
        output = new WaveOutEvent();
        output.Init(reader);
    }

    public static WindowsAnimationAudioPlayback Create(string path, double offset)
    {
        WaveStream reader = Path.GetExtension(path).Equals(".ogg", StringComparison.OrdinalIgnoreCase)
            ? new VorbisWaveReader(path)
            : new AudioFileReader(path);
        reader.CurrentTime = TimeSpan.FromSeconds(Math.Max(0, offset));
        WindowsAnimationAudioPlayback playback = new(reader);
        playback.output.Play();
        return playback;
    }

    public static double GetDuration(string path)
    {
        using WaveStream reader = Path.GetExtension(path).Equals(".ogg", StringComparison.OrdinalIgnoreCase)
            ? new VorbisWaveReader(path)
            : new AudioFileReader(path);
        return reader.TotalTime.TotalSeconds;
    }

    public void Dispose()
    {
        output.Stop();
        output.Dispose();
        reader.Dispose();
    }
}
