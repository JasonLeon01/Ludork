using Avalonia.Platform;
using Ludork.EditorTutorial.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Ludork.EditorTutorial.Playback;

public sealed class TutorialCatalog
{
    private static readonly Uri scenesUri = new("avares://Ludork/Editor/EditorTutorial/Scenes/");
    private static readonly Uri assetsUri = new("avares://Ludork/Editor/EditorTutorial/Assets/");
    private static readonly JsonSerializerOptions jsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
    };
    private readonly Dictionary<string, TutorialScene> scenes;

    private TutorialCatalog(Dictionary<string, TutorialScene> scenes)
    {
        this.scenes = scenes;
    }

    public TutorialScene? GetScene(string trigger)
    {
        return scenes.GetValueOrDefault(trigger);
    }

    public static TutorialCatalog Load(Action<string> reportError)
    {
        List<TutorialScene> loaded = [];
        foreach (Uri uri in AssetLoader.GetAssets(scenesUri, null).OrderBy(value => value.AbsoluteUri, StringComparer.Ordinal))
        {
            if (!uri.AbsolutePath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
                continue;

            try
            {
                using Stream stream = AssetLoader.Open(uri);
                using StreamReader reader = new(stream);
                loaded.Add(DeserializeScene(reader.ReadToEnd()));
            }
            catch (Exception exception) when (exception is JsonException or InvalidDataException or IOException or ArgumentException)
            {
                reportError($"Editor tutorial '{uri.AbsolutePath}' is disabled: {exception.Message}");
            }
        }

        return Create(loaded, reportError);
    }

    public static TutorialScene ParseScene(string json)
    {
        TutorialScene scene = DeserializeScene(json);
        Validate(scene);
        return scene;
    }

    public static TutorialCatalog Create(IEnumerable<TutorialScene> candidates, Action<string> reportError)
    {
        List<TutorialScene> loaded = candidates.ToList();
        HashSet<string> duplicateIds = loaded.Where(scene => scene is not null && IsIdentifier(scene.Id))
            .GroupBy(scene => scene.Id, StringComparer.OrdinalIgnoreCase)
            .Where(group => group.Count() > 1).Select(group => group.Key).ToHashSet(StringComparer.OrdinalIgnoreCase);
        HashSet<string> duplicateTriggers = loaded.Where(scene => scene is not null && IsIdentifier(scene.Trigger))
            .GroupBy(scene => scene.Trigger, StringComparer.Ordinal)
            .Where(group => group.Count() > 1).Select(group => group.Key).ToHashSet(StringComparer.Ordinal);
        Dictionary<string, TutorialScene> available = new(StringComparer.Ordinal);
        foreach (TutorialScene scene in loaded)
        {
            try
            {
                Validate(scene);
                if (duplicateIds.Contains(scene.Id) || duplicateTriggers.Contains(scene.Trigger))
                    throw new InvalidDataException($"Duplicate scene ID or trigger '{scene.Trigger}'. Give every scene a unique ID and trigger.");

                available.Add(scene.Trigger, scene);
            }
            catch (InvalidDataException exception)
            {
                reportError($"Editor tutorial '{scene?.Id ?? "<null>"}' is disabled: {exception.Message}");
            }
        }

        return new TutorialCatalog(available);
    }

    public static Uri GetAssetUri(string asset)
    {
        if (string.IsNullOrWhiteSpace(asset)
            || asset.StartsWith('/') || asset.Contains('\\')
            || asset.Split('/').Any(segment => segment is "" or "." or "..")
            || asset.IndexOfAny([':', '?', '#', '%']) >= 0)
            throw new InvalidDataException($"Image asset '{asset}' must be a relative path inside EditorTutorial/Assets.");

        return new Uri(assetsUri, asset);
    }

    private static TutorialScene DeserializeScene(string json)
    {
        return JsonSerializer.Deserialize<TutorialScene>(json, jsonOptions)
            ?? throw new InvalidDataException("The scene must be a JSON object.");
    }

    private static void Validate(TutorialScene scene)
    {
        if (scene is null)
            throw new InvalidDataException("The scene must be a JSON object.");
        if (!IsIdentifier(scene.Id) || !IsIdentifier(scene.Trigger) || !IsIdentifier(scene.Anchor))
            throw new InvalidDataException("Scene id, trigger and anchor must contain only letters, digits, dots, hyphens or underscores.");
        if (!double.IsFinite(scene.Duration) || scene.Duration <= 0)
            throw new InvalidDataException("Scene duration must be a finite number greater than zero.");
        if (scene.Elements is null || scene.Elements.Count == 0 || scene.Tracks is null)
            throw new InvalidDataException("A scene needs at least one element and a non-null tracks array.");

        HashSet<string> elementIds = new(StringComparer.Ordinal);
        ValidateElements(scene.Elements, elementIds);
        HashSet<(string Target, string Property)> trackTargets = [];
        foreach (TutorialTrack track in scene.Tracks)
        {
            if (track is null || track.Target is null || !elementIds.Contains(track.Target))
                throw new InvalidDataException($"Track target '{track?.Target}' does not name an element in this scene.");
            if (track.Property is not ("x" or "y" or "width" or "height" or "opacity" or "scaleX" or "scaleY" or "rotation"))
                throw new InvalidDataException($"Track '{track.Target}' has unsupported property '{track.Property}'.");
            if (!trackTargets.Add((track.Target, track.Property)))
                throw new InvalidDataException($"Element '{track.Target}' has more than one track for '{track.Property}'.");
            if (track.Keyframes is null || track.Keyframes.Count == 0)
                throw new InvalidDataException($"Track '{track.Target}.{track.Property}' needs at least one keyframe.");

            double previousTime = -1;
            foreach (TutorialKeyframe keyframe in track.Keyframes)
            {
                if (keyframe is null || !double.IsFinite(keyframe.Time)
                    || keyframe.Time < 0 || keyframe.Time > scene.Duration || keyframe.Time <= previousTime)
                    throw new InvalidDataException($"Track '{track.Target}.{track.Property}' keyframe times must increase strictly from 0 through {scene.Duration}.");
                if (keyframe.Easing is not ("linear" or "smooth"))
                    throw new InvalidDataException($"Track '{track.Target}.{track.Property}' uses unsupported easing '{keyframe.Easing}'. Use linear or smooth.");
                ValidateValue(new TutorialValue(keyframe.Value, keyframe.Offset), track.Property, track.Target);
                previousTime = keyframe.Time;
            }
        }
    }

    private static void ValidateElements(IEnumerable<TutorialElement> elements, HashSet<string> ids)
    {
        foreach (TutorialElement element in elements)
        {
            if (element is null || !IsIdentifier(element.Id) || !ids.Add(element.Id))
                throw new InvalidDataException($"Element ID '{element?.Id}' must be valid and unique within the scene.");
            if (element.Type is not ("group" or "text" or "image" or "rectangle" or "ellipse" or "line"))
                throw new InvalidDataException($"Element '{element.Id}' has unsupported type '{element.Type}'.");
            if (element.Children is null || (element.Type != "group" && element.Children.Count != 0))
                throw new InvalidDataException($"Element '{element.Id}' must have a non-null children array; only groups can contain children.");
            if (element.Type == "text" && string.IsNullOrWhiteSpace(element.TextKey))
                throw new InvalidDataException($"Text element '{element.Id}' needs a locale textKey.");
            if (element.TextArguments is null || element.TextArguments.Any(argument =>
                argument is not ("primaryModifier" or "alternateModifier"))
                || element.Type != "text" && element.TextArguments.Count != 0)
                throw new InvalidDataException($"Element '{element.Id}' has invalid textArguments.");
            if (element.LayoutKey is not null && !IsIdentifier(element.LayoutKey))
                throw new InvalidDataException($"Element '{element.Id}' has an invalid layoutKey.");
            if (element.Stretch is not ("fill" or "uniform")
                || element.ImageVerticalAlignment is not ("top" or "center" or "bottom"))
                throw new InvalidDataException($"Element '{element.Id}' has an unsupported image layout.");
            if (element.Type == "image")
            {
                if ((element.Asset is null) == (element.ImageKey is null))
                    throw new InvalidDataException($"Image element '{element.Id}' needs exactly one asset or imageKey.");
                if (element.ImageKey is not null && !IsIdentifier(element.ImageKey))
                    throw new InvalidDataException($"Image element '{element.Id}' has an invalid imageKey.");
                if (element.Asset is not null)
                {
                    Uri uri = GetAssetUri(element.Asset);
                    if (!AssetLoader.Exists(uri))
                        throw new InvalidDataException($"Image element '{element.Id}' references missing asset '{element.Asset}'. Include it as an Avalonia resource.");
                }
            }
            else if (element.ImageKey is not null || element.Asset is not null)
                throw new InvalidDataException($"Only image elements can reference an asset or imageKey.");

            ValidateValue(element.X, "x", element.Id);
            ValidateValue(element.Y, "y", element.Id);
            ValidateValue(element.Width, "width", element.Id);
            ValidateValue(element.Height, "height", element.Id);
            ValidateValue(new TutorialValue(element.Opacity), "opacity", element.Id);
            ValidateValue(new TutorialValue(element.ScaleX), "scaleX", element.Id);
            ValidateValue(new TutorialValue(element.ScaleY), "scaleY", element.Id);
            ValidateValue(new TutorialValue(element.Rotation), "rotation", element.Id);
            if (!double.IsFinite(element.FontSize) || element.FontSize <= 0
                || !double.IsFinite(element.StrokeThickness) || element.StrokeThickness < 0)
                throw new InvalidDataException($"Element '{element.Id}' needs a positive finite fontSize and non-negative finite strokeThickness.");
            ValidateElements(element.Children, ids);
        }
    }

    private static void ValidateValue(TutorialValue value, string property, string elementId)
    {
        bool invalid = !double.IsFinite(value.Value) || !double.IsFinite(value.Offset);
        if (property is "width" or "height")
            invalid |= value.Value < 0 || (value.Value == 0 && value.Offset < 0);
        if (property is "opacity" or "scaleX" or "scaleY" or "rotation")
            invalid |= value.Offset != 0;
        if (property == "opacity")
            invalid |= value.Value < 0 || value.Value > 1;
        if (property is "scaleX" or "scaleY")
            invalid |= value.Value < 0;
        if (invalid)
            throw new InvalidDataException($"Element '{elementId}' has an invalid '{property}' value. Use finite coordinates, non-negative dimensions and scales, and opacity from 0 to 1; only coordinates and dimensions accept offsets.");
    }

    private static bool IsIdentifier(string? value)
    {
        return !string.IsNullOrWhiteSpace(value)
            && value.All(character => char.IsAsciiLetterOrDigit(character) || character is '.' or '-' or '_');
    }
}
