using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class AnimationTimeTagCache
{
    private const double Tolerance = 0.000001;
    private readonly List<AnimationTimeTagLayout> items = [];
    private readonly Dictionary<int, AnimationTimeTagLayout> lookup = [];
    private readonly TimelineIntervalIndex<AnimationTimeTagLayout> times = new([], item => item.Time, item => item.Time);
    private readonly TimelineIntervalIndex<AnimationTimeTagLayout> visuals = new([], item => item.Left, item => item.Right);
    private readonly List<AnimationTimeTagLayout> neighbors = [];
    private readonly HashSet<AnimationTimeTagLayout> affected = [];
    private double scale;

    public double MaximumTime => times.MaximumEnd;

    public void Reset(JsonArray tags, double pixelsPerSecond)
    {
        scale = pixelsPerSecond;
        items.Clear();
        lookup.Clear();
        int first = 0;
        for (int index = 0; index < tags.Count; index++)
        {
            if (tags[index] is not JsonObject data)
                continue;
            double time = JsonScalar.Number(data["time"]);
            while (first < items.Count && time - items[first].Time >= Tolerance)
                first++;
            AnimationTimeTagLayout item = new(index, data, time)
            {
                DuplicateCount = items.Count - first,
            };
            item.SetScale(scale);
            items.Add(item);
            lookup[index] = item;
        }
        times.Reset(items);
        visuals.Reset(items);
    }

    public void RefreshText()
    {
        foreach (AnimationTimeTagLayout item in items)
            item.RefreshText();
        visuals.Reset(items);
    }

    public void SetScale(double pixelsPerSecond)
    {
        scale = pixelsPerSecond;
        foreach (AnimationTimeTagLayout item in items)
            item.SetScale(scale);
        visuals.Reset(items);
    }

    public void UpdateTime(int index)
    {
        if (!lookup.TryGetValue(index, out AnimationTimeTagLayout? changed))
            return;
        double previousTime = changed.Time;
        double nextTime = JsonScalar.Number(changed.Data["time"]);
        if (previousTime == nextTime)
            return;
        affected.Clear();
        times.Query(previousTime - Tolerance, previousTime + Tolerance, neighbors);
        affected.UnionWith(neighbors);
        times.Query(nextTime - Tolerance, nextTime + Tolerance, neighbors);
        affected.UnionWith(neighbors);
        int duplicateCount = 0;
        foreach (AnimationTimeTagLayout item in affected)
        {
            if (item.Index < changed.Index && Math.Abs(item.Time - nextTime) < Tolerance)
                duplicateCount++;
            if (item.Index <= changed.Index)
                continue;
            int delta = (Math.Abs(item.Time - nextTime) < Tolerance ? 1 : 0)
                - (Math.Abs(item.Time - previousTime) < Tolerance ? 1 : 0);
            if (delta == 0)
                continue;
            item.DuplicateCount += delta;
            item.SetScale(scale);
            visuals.Update(item);
        }
        changed.Time = nextTime;
        changed.DuplicateCount = duplicateCount;
        changed.SetScale(scale);
        times.Update(changed);
        visuals.Update(changed);
        affected.Clear();
        neighbors.Clear();
    }

    public void Query(double left, double right, List<AnimationTimeTagLayout> destination)
    {
        visuals.Query(left, right, destination);
    }
}
