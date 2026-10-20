using Ludork.EditorTutorial.Models;
using System;

namespace Ludork.EditorTutorial.Playback;

public static class TutorialTimeline
{
    public static double NormalizeTime(TutorialScene scene, double elapsed)
    {
        double time = Math.Max(0, elapsed);
        return scene.Loop ? time % scene.Duration : Math.Min(time, scene.Duration);
    }

    public static TutorialValue Evaluate(TutorialTrack track, double time)
    {
        TutorialKeyframe first = track.Keyframes[0];
        if (time <= first.Time)
            return new TutorialValue(first.Value, first.Offset);

        for (int index = 1; index < track.Keyframes.Count; index++)
        {
            TutorialKeyframe destination = track.Keyframes[index];
            if (time > destination.Time)
                continue;

            TutorialKeyframe source = track.Keyframes[index - 1];
            double progress = (time - source.Time) / (destination.Time - source.Time);
            if (destination.Easing == "smooth")
                progress = progress * progress * (3 - 2 * progress);

            return new TutorialValue(
                source.Value + (destination.Value - source.Value) * progress,
                source.Offset + (destination.Offset - source.Offset) * progress);
        }

        TutorialKeyframe last = track.Keyframes[^1];
        return new TutorialValue(last.Value, last.Offset);
    }
}
