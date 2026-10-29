using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class AnimationTimelineIndex
{
    private readonly List<List<AnimationTimelineSegment>> tracks = [];
    private readonly List<TimelineIntervalIndex<AnimationTimelineSegment>> trackIndexes = [];
    private readonly Dictionary<(int Track, int Segment), AnimationTimelineSegment> lookup = [];
    private readonly TimelineIntervalIndex<AnimationTimelineSegment> all = new([], item => item.Start, item => item.End,
        Comparer<AnimationTimelineSegment>.Create(static (left, right) =>
        {
            int track = left.Track.CompareTo(right.Track);
            return track != 0 ? track : left.Index.CompareTo(right.Index);
        }));
    private readonly List<AnimationTimelineSegment> active = [];
    private double activeTime = double.NaN;
    private long activeRevision = -1;

    public int TrackCount => tracks.Count;
    public double Duration => Math.Max(0, all.MaximumEnd);
    public long Revision { get; private set; }

    public void Reset(JsonObject data)
    {
        tracks.Clear();
        trackIndexes.Clear();
        lookup.Clear();
        all.Reset([]);
        JsonArray? source = data["timeLines"] as JsonArray;
        if (source is not null)
        {
            for (int track = 0; track < source.Count; track++)
                addTrack(track, source[track]);
        }
        Revision++;
    }

    public void RefreshTrack(JsonObject data, int track)
    {
        JsonArray? source = data["timeLines"] as JsonArray;
        if (source is null || source.Count < tracks.Count || track < 0 || track >= source.Count)
        {
            Reset(data);
            return;
        }
        while (tracks.Count < source.Count)
            addTrack(tracks.Count, source[tracks.Count]);
        foreach (AnimationTimelineSegment item in tracks[track])
        {
            lookup.Remove((item.Track, item.Index));
            all.Remove(item);
        }
        List<AnimationTimelineSegment> replacements = readTrack(track, source[track]);
        tracks[track] = replacements;
        trackIndexes[track].Reset(replacements);
        foreach (AnimationTimelineSegment item in replacements)
            all.Add(item);
        Revision++;
    }

    public void Update(int track, int segment)
    {
        if (!lookup.TryGetValue((track, segment), out AnimationTimelineSegment? item))
            return;
        trackIndexes[track].Update(item);
        all.Update(item);
        Revision++;
    }

    public void QueryTrack(int track, double start, double end, List<AnimationTimelineSegment> destination)
    {
        destination.Clear();
        if (track >= 0 && track < trackIndexes.Count)
            trackIndexes[track].Query(start, end, destination);
    }

    public IReadOnlyList<AnimationTimelineSegment> ActiveAt(double time)
    {
        if (activeTime != time || activeRevision != Revision)
        {
            all.Query(time, time, active);
            activeTime = time;
            activeRevision = Revision;
        }
        return active;
    }

    private void addTrack(int track, JsonNode? source)
    {
        List<AnimationTimelineSegment> items = readTrack(track, source);
        tracks.Add(items);
        trackIndexes.Add(new TimelineIntervalIndex<AnimationTimelineSegment>(items, item => item.Start, item => item.End));
        foreach (AnimationTimelineSegment item in items)
            all.Add(item);
    }

    private List<AnimationTimelineSegment> readTrack(int track, JsonNode? source)
    {
        List<AnimationTimelineSegment> items = [];
        if (source is JsonObject line && line["timeSegments"] is JsonArray segments)
        {
            for (int index = 0; index < segments.Count; index++)
            {
                if (segments[index] is not JsonObject segment)
                    continue;
                AnimationTimelineSegment item = new(track, index, segment);
                items.Add(item);
                lookup[(track, index)] = item;
            }
        }
        return items;
    }
}
