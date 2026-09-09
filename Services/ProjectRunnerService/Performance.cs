using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Services;

public sealed partial class ProjectRunnerService
{
    private void writeOutput(string line)
    {
        if (tryParsePerformanceSample(line, out PerformanceSample sample))
        {
            PerformanceSampleReceived?.Invoke(this, sample);
            return;
        }
        OutputReceived?.Invoke(this, line);
    }

    private static bool tryParsePerformanceSample(string line, out PerformanceSample sample)
    {
        sample = null!;
        if (!line.StartsWith(PerformanceSamplePrefix, StringComparison.Ordinal))
            return false;
        string json = line[PerformanceSamplePrefix.Length..].Trim();
        try
        {
            using JsonDocument document = JsonDocument.Parse(json);
            JsonElement root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object
                || !tryReadInteger(root, "v", out int version)
                || version != BridgeProtocolVersion
                || !tryReadDouble(root, "fps", out double fps)
                || !tryReadDouble(root, "memory", out double memory)
                || !tryReadMainFrames(root, out IReadOnlyList<MainFrameTiming> mainFrames)
                || !tryReadLogicTicks(root, out IReadOnlyList<LogicTickTiming> logicTicks))
            {
                return false;
            }

            int sampleFrames = 30;
            if (root.TryGetProperty("sampleFrames", out JsonElement sampleFramesElement)
                && (!tryReadInteger(sampleFramesElement, out sampleFrames) || sampleFrames <= 0))
            {
                return false;
            }

            double? targetFps = null;
            if (root.TryGetProperty("targetFps", out JsonElement targetFpsElement)
                && targetFpsElement.ValueKind != JsonValueKind.Null)
            {
                if (!tryReadDouble(targetFpsElement, out double targetValue) || targetValue <= 0)
                    return false;
                targetFps = targetValue;
            }

            long droppedLogicTicks = 0;
            if (root.TryGetProperty("droppedLogicTicks", out JsonElement droppedElement)
                && (!tryReadLong(droppedElement, out droppedLogicTicks) || droppedLogicTicks < 0))
            {
                return false;
            }

            sample = new PerformanceSample(fps, memory)
            {
                ProtocolVersion = version,
                SampleFrames = sampleFrames,
                TargetFps = targetFps,
                MainFrames = mainFrames,
                LogicTicks = logicTicks,
                DroppedLogicTicks = droppedLogicTicks,
            };
            return true;
        }
        catch (JsonException)
        {
            return false;
        }
    }

    private static bool tryReadDouble(JsonElement root, string name, out double value)
    {
        value = 0;
        if (!root.TryGetProperty(name, out JsonElement element))
            return false;
        return tryReadDouble(element, out value);
    }

    private static bool tryReadDouble(JsonElement element, out double value)
    {
        value = 0;
        return element.ValueKind == JsonValueKind.Number
            && element.TryGetDouble(out value)
            && double.IsFinite(value);
    }

    private static bool tryReadDuration(JsonElement root, string name, out double value)
    {
        return tryReadDouble(root, name, out value) && value >= 0;
    }

    private static bool tryReadInteger(JsonElement root, string name, out int value)
    {
        value = 0;
        return root.TryGetProperty(name, out JsonElement element)
            && tryReadInteger(element, out value);
    }

    private static bool tryReadInteger(JsonElement element, out int value)
    {
        value = 0;
        return element.ValueKind == JsonValueKind.Number && element.TryGetInt32(out value);
    }

    private static bool tryReadLong(JsonElement element, out long value)
    {
        value = 0;
        return element.ValueKind == JsonValueKind.Number && element.TryGetInt64(out value);
    }

    private static bool tryReadLong(JsonElement root, string name, out long value)
    {
        value = 0;
        return root.TryGetProperty(name, out JsonElement element)
            && tryReadLong(element, out value);
    }

    private static bool tryReadMainFrames(
        JsonElement root,
        out IReadOnlyList<MainFrameTiming> frames)
    {
        frames = Array.Empty<MainFrameTiming>();
        if (!root.TryGetProperty("mainFrames", out JsonElement values))
            return true;
        if (values.ValueKind != JsonValueKind.Array)
            return false;
        List<MainFrameTiming> result = new(values.GetArrayLength());
        foreach (JsonElement value in values.EnumerateArray())
        {
            if (value.ValueKind != JsonValueKind.Object
                || !tryReadDuration(value, "time", out double time)
                || !tryReadDuration(value, "interval", out double interval)
                || !tryReadDuration(value, "active", out double active)
                || !tryReadDuration(value, "runtime", out double runtime)
                || !tryReadDuration(value, "sceneOps", out double sceneOps)
                || !tryReadDuration(value, "input", out double input)
                || !tryReadDuration(value, "uiUpdate", out double uiUpdate)
                || !tryReadDuration(value, "renderCpu", out double renderCpu)
                || !tryReadDuration(value, "presentWait", out double presentWait)
                || !tryReadDuration(value, "lateUpdate", out double lateUpdate)
                || !tryReadDuration(value, "audio", out double audio))
            {
                continue;
            }
            result.Add(new MainFrameTiming(
                time,
                interval,
                active,
                runtime,
                sceneOps,
                input,
                uiUpdate,
                renderCpu,
                presentWait,
                lateUpdate,
                audio));
        }
        frames = result;
        return true;
    }

    private static bool tryReadLogicTicks(
        JsonElement root,
        out IReadOnlyList<LogicTickTiming> ticks)
    {
        ticks = Array.Empty<LogicTickTiming>();
        if (!root.TryGetProperty("logicTicks", out JsonElement values))
            return true;
        if (values.ValueKind != JsonValueKind.Array)
            return false;
        List<LogicTickTiming> result = new(values.GetArrayLength());
        foreach (JsonElement value in values.EnumerateArray())
        {
            if (value.ValueKind != JsonValueKind.Object
                || !tryReadDuration(value, "time", out double time)
                || !tryReadDuration(value, "iteration", out double iteration)
                || !tryReadDuration(value, "sceneTick", out double sceneTick)
                || !tryReadDuration(value, "maintenance", out double maintenance)
                || !tryReadDuration(value, "fixedTick", out double fixedTick)
                || !tryReadDuration(value, "sleep", out double sleep)
                || !tryReadInteger(value, "fixedSteps", out int fixedSteps)
                || fixedSteps < 0
                || !tryReadWorldStreaming(value, out WorldStreamingTiming? worldStreaming))
            {
                continue;
            }
            result.Add(new LogicTickTiming(
                time,
                iteration,
                sceneTick,
                maintenance,
                fixedTick,
                sleep,
                fixedSteps,
                worldStreaming));
        }
        ticks = result;
        return true;
    }

    private static bool tryReadWorldStreaming(
        JsonElement tick,
        out WorldStreamingTiming? timing)
    {
        timing = null;
        if (!tick.TryGetProperty("worldStreaming", out JsonElement value))
            return true;
        if (value.ValueKind != JsonValueKind.Object
            || !tryReadInteger(value, "queueDepth", out int queueDepth)
            || !tryReadInteger(value, "reading", out int reading)
            || !tryReadInteger(value, "prepared", out int prepared)
            || !tryReadInteger(value, "active", out int active)
            || !tryReadInteger(value, "dormant", out int dormant)
            || !tryReadLong(value, "cacheBytes", out long cacheBytes)
            || !tryReadDuration(value, "publishMilliseconds", out double publishMilliseconds)
            || !tryReadInteger(value, "visibleTileChunks", out int visibleTileChunks)
            || !tryReadInteger(value, "activeActors", out int activeActors)
            || queueDepth < 0
            || reading < 0
            || prepared < 0
            || active < 0
            || dormant < 0
            || cacheBytes < 0
            || visibleTileChunks < 0
            || activeActors < 0)
        {
            return false;
        }
        timing = new WorldStreamingTiming(
            queueDepth,
            reading,
            prepared,
            active,
            dormant,
            cacheBytes,
            publishMilliseconds,
            visibleTileChunks,
            activeActors);
        return true;
    }

    private void setState(ProjectRunState state)
    {
        if (State == state)
            return;
        State = state;
        StateChanged?.Invoke(this, state);
    }

    private void setCanSendCommand(bool value)
    {
        if (CanSendCommand == value)
            return;
        CanSendCommand = value;
        CommandAvailabilityChanged?.Invoke(this, value);
    }

}

