using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class ParticleAssetSchema
{
    public static JsonObject CreateTrack(string name)
    {
        return new JsonObject
        {
            ["name"] = name, ["enabled"] = true, ["mode"] = "emission", ["duration"] = 2,
            ["delay"] = 0, ["loop"] = true, ["prewarm"] = false, ["capacity"] = 1024,
            ["count"] = 32, ["rate"] = 30, ["distanceRate"] = 0, ["bursts"] = new JsonArray(),
            ["shape"] = "point", ["extent"] = Array(32, 32), ["radius"] = 16, ["innerRadius"] = 8,
            ["direction"] = -90, ["spread"] = 30, ["lifetime"] = Array(1, 2), ["speed"] = Array(20, 40),
            ["sizeMin"] = Array(8, 8), ["sizeMax"] = Array(16, 16), ["rotation"] = Array(0, 360),
            ["angularVelocity"] = Array(0, 0), ["colourMin"] = Array(255, 255, 255, 255),
            ["colourMax"] = Array(255, 255, 255, 255), ["gravity"] = Array(0, 0),
            ["radialAcceleration"] = 0, ["tangentialAcceleration"] = 0, ["damping"] = 0,
            ["texture"] = "", ["textureRect"] = Array(0, 0, 0, 0), ["columns"] = 1, ["rows"] = 1,
            ["frameCount"] = 1, ["frameRate"] = 0, ["randomStartFrame"] = false, ["frameLoop"] = true,
            ["space"] = "local", ["scaleMode"] = "hierarchy", ["blend"] = "alpha",
            ["offset"] = Array(0, 0), ["rotationOffset"] = 0, ["scale"] = Array(1, 1),
            ["curves"] = new JsonObject(),
        };
    }

    public static JsonObject CreateCurve(string channel)
    {
        double value = channel == "rotation" ? 0 : 1;
        return new JsonObject
        {
            ["defaultValue"] = value, ["preInfinity"] = "constant", ["postInfinity"] = "constant",
            ["keys"] = new JsonArray(Key(0, value), Key(1, value)),
        };
    }

    public static JsonArray Array(params double[] values)
    {
        JsonArray result = new();
        foreach (double value in values)
            result.Add(value);
        return result;
    }

    public static JsonObject Key(double time, double value)
    {
        return new JsonObject
        {
            ["time"] = time, ["value"] = value, ["interpolation"] = "linear",
            ["arriveTangent"] = 0, ["leaveTangent"] = 0,
        };
    }

    public static double Number(JsonNode? node, double fallback = 0)
    {
        if (node is not JsonValue value)
            return fallback;
        if (value.TryGetValue<double>(out double real))
            return real;
        if (value.TryGetValue<int>(out int integer))
            return integer;
        if (value.TryGetValue<long>(out long wide))
            return wide;
        if (value.TryGetValue<decimal>(out decimal precise))
            return (double)precise;
        return fallback;
    }

    public static IReadOnlyList<string> Validate(JsonObject asset, string key)
    {
        List<string> errors = [];
        string root = "Particles/" + key;
        validateChoice(asset, "type", ["particle"], root, errors);
        validateNumber(asset, "simulationRate", 60, 1, 240, true, root, errors);
        validateNumber(asset, "seed", 1, 0, 16777215, true, root, errors);
        if (!asset.ContainsKey("tracks"))
            return errors;
        if (asset["tracks"] is not JsonArray tracks)
        {
            errors.Add(root + ".tracks must be an array");
            return errors;
        }
        HashSet<string> names = new(StringComparer.Ordinal);
        JsonObject defaults = CreateTrack("Track");
        for (int index = 0; index < tracks.Count; index++)
        {
            string path = root + $".tracks[{index}]";
            if (tracks[index] is not JsonObject track)
            {
                errors.Add(path + " must be an object");
                continue;
            }
            validateFields(track, defaults, path, errors);
            string name = track["name"] is JsonValue nameValue && nameValue.TryGetValue<string>(out string? parsedName) ? parsedName : "Track";
            if (!names.Add(name))
                errors.Add(path + ".name duplicates another track");
            string? texture = track["texture"] is JsonValue textureValue && textureValue.TryGetValue<string>(out string? parsedTexture) ? parsedTexture : null;
            if (!string.IsNullOrEmpty(texture) && !GameAssetPath.IsCanonical(texture))
                errors.Add(path + ".texture must use a canonical /Game/Assets/ path");
            validateChoice(track, "mode", ["emission", "resident"], path, errors);
            validateChoice(track, "space", ["local", "world"], path, errors);
            validateChoice(track, "blend", ["alpha", "add"], path, errors);
            validateChoice(track, "scaleMode", ["hierarchy", "local", "shape"], path, errors);
            validateChoice(track, "shape", ["point", "line", "rectangle", "disk", "ring"], path, errors);
            foreach (string range in new[] { "lifetime", "speed", "rotation", "angularVelocity" })
            {
                if (component(track, defaults, range, 0) > component(track, defaults, range, 1))
                    errors.Add(path + "." + range + " minimum exceeds maximum");
            }
            if (component(track, defaults, "lifetime", 0) <= 0)
                errors.Add(path + ".lifetime must be positive");
            for (int axis = 0; axis < 2; axis++)
            {
                if (component(track, defaults, "sizeMin", axis) < 0
                    || component(track, defaults, "sizeMin", axis) > component(track, defaults, "sizeMax", axis))
                    errors.Add(path + ".sizeMin/sizeMax contains an invalid range");
                if (component(track, defaults, "extent", axis) < 0)
                    errors.Add(path + ".extent must be nonnegative");
            }
            for (int channel = 0; channel < 4; channel++)
            {
                double minimum = component(track, defaults, "colourMin", channel);
                double maximum = component(track, defaults, "colourMax", channel);
                if (minimum < 0 || maximum > 255 || minimum > maximum)
                    errors.Add(path + ".colourMin/colourMax must be ordered within [0,255]");
                double rectangle = component(track, defaults, "textureRect", channel);
                if (rectangle < 0 || rectangle > int.MaxValue || rectangle != Math.Truncate(rectangle))
                    errors.Add(path + ".textureRect must contain nonnegative integers within the native int range");
            }
            if ((component(track, defaults, "textureRect", 2) == 0) != (component(track, defaults, "textureRect", 3) == 0))
                errors.Add(path + ".textureRect width and height must both be zero or both be positive");
            validateNumber(track, "capacity", 1024, 1, 1000000, true, path, errors);
            validateNumber(track, "count", Math.Min(32, Number(track["capacity"], 1024)), 0,
                Number(track["capacity"], 1024), true, path, errors);
            validateNumber(track, "duration", 2, 0, float.MaxValue, false, path, errors, true);
            validateNumber(track, "columns", 1, 1, 4096, true, path, errors);
            validateNumber(track, "rows", 1, 1, 4096, true, path, errors);
            validateNumber(track, "frameCount", 1, 1, Number(track["columns"], 1) * Number(track["rows"], 1), true, path, errors);
            foreach (string property in new[] { "delay", "rate", "distanceRate", "radius", "innerRadius", "spread", "damping", "frameRate" })
                validateNumber(track, property, Number(defaults[property]), 0, float.MaxValue, false, path, errors);
            if (track["shape"] is JsonValue shape && shape.TryGetValue<string>(out string? shapeName) && shapeName == "ring"
                && (float)Number(track["innerRadius"], 8) > (float)Number(track["radius"], 16))
                errors.Add(path + ".innerRadius exceeds ring radius");
            validateBursts(track, path, errors);
            validateCurves(track, path, errors);
        }
        return errors;
    }

    private static void validateFields(JsonObject owner, JsonObject defaults, string path, ICollection<string> errors)
    {
        foreach (KeyValuePair<string, JsonNode?> field in defaults)
        {
            if (!owner.TryGetPropertyValue(field.Key, out JsonNode? value))
                continue;
            if (field.Value is JsonValue scalar)
            {
                if (scalar.TryGetValue<string>(out _))
                {
                    if (value is not JsonValue text || !text.TryGetValue<string>(out _))
                        errors.Add(path + "." + field.Key + " must be a string");
                }
                else if (scalar.TryGetValue<bool>(out _))
                {
                    if (value is not JsonValue flag || !flag.TryGetValue<bool>(out _))
                        errors.Add(path + "." + field.Key + " must be a boolean");
                }
                else
                    validateNumber(owner, field.Key, Number(field.Value), -float.MaxValue, float.MaxValue, false, path, errors);
            }
            else if (field.Value is JsonArray expected && field.Key is not "bursts" and not "keys"
                && (value is not JsonArray vector || vector.Count != expected.Count
                    || vector.Any(item => !double.IsFinite(Number(item, double.NaN)) || Math.Abs(Number(item)) > float.MaxValue)))
                errors.Add(path + "." + field.Key + " has invalid components");
        }
    }

    private static double component(JsonObject owner, JsonObject defaults, string property, int index)
    {
        JsonNode? value = owner.ContainsKey(property) ? owner[property] : defaults[property];
        return value is JsonArray vector && index < vector.Count ? (float)Number(vector[index], double.NaN) : double.NaN;
    }

    private static void validateChoice(JsonObject owner, string property, string[] choices, string path, ICollection<string> errors)
    {
        if (owner.TryGetPropertyValue(property, out JsonNode? value)
            && (value is not JsonValue scalar || !scalar.TryGetValue<string>(out string? choice) || !choices.Contains(choice, StringComparer.Ordinal)))
            errors.Add(path + "." + property + " must be one of: " + string.Join(", ", choices));
    }

    private static void validateBursts(JsonObject track, string path, ICollection<string> errors)
    {
        if (!track.ContainsKey("bursts"))
            return;
        if (track["bursts"] is not JsonArray bursts)
        {
            errors.Add(path + ".bursts must be an array");
            return;
        }
        for (int index = 0; index < bursts.Count; index++)
        {
            string burstPath = path + $".bursts[{index}]";
            if (bursts[index] is not JsonObject burst)
            {
                errors.Add(burstPath + " must be an object");
                continue;
            }
            validateNumber(burst, "time", 0, 0, float.MaxValue, false, burstPath, errors);
            if ((float)Number(burst["time"]) >= (float)Number(track["duration"], 2))
                errors.Add(burstPath + ".time must be less than track duration");
            validateNumber(burst, "count", 0, 0, 1000000, true, burstPath, errors);
            validateNumber(burst, "cycles", 1, 1, 10000, true, burstPath, errors);
            validateNumber(burst, "interval", 0, 0, float.MaxValue, false, burstPath, errors, Number(burst["cycles"], 1) > 1);
        }
    }

    private static void validateCurves(JsonObject track, string path, ICollection<string> errors)
    {
        if (!track.ContainsKey("curves"))
            return;
        if (track["curves"] is not JsonObject curves)
        {
            errors.Add(path + ".curves must be an object");
            return;
        }
        foreach (string channel in new[] { "speed", "sizeX", "sizeY", "rotation", "red", "green", "blue", "alpha" })
        {
            if (!curves.TryGetPropertyValue(channel, out JsonNode? value))
                continue;
            if (value is JsonValue reference && reference.TryGetValue<string>(out _))
                continue;
            string curvePath = path + ".curves." + channel;
            if (value is not JsonObject curve)
            {
                errors.Add(curvePath + " must be a curve object or resource key");
                continue;
            }
            validateFields(curve, CreateCurve(channel), curvePath, errors);
            if (!curve.ContainsKey("keys"))
                continue;
            if (curve["keys"] is not JsonArray keys)
            {
                errors.Add(curvePath + ".keys must be an array");
                continue;
            }
            for (int index = 0; index < keys.Count; index++)
            {
                string keyPath = curvePath + $".keys[{index}]";
                if (keys[index] is not JsonObject entry)
                {
                    errors.Add(keyPath + " must be an object");
                    continue;
                }
                validateFields(entry, Key(0, channel == "rotation" ? 0 : 1), keyPath, errors);
                validateNumber(entry, "time", 0, 0, 1, false, keyPath, errors);
            }
        }
    }

    private static void validateNumber(JsonObject owner, string property, double fallback,
        double minimum, double maximum, bool integer, string path, ICollection<string> errors, bool exclusiveMinimum = false)
    {
        double value = owner.ContainsKey(property) ? Number(owner[property], double.NaN) : fallback;
        if (!integer)
        {
            if (Math.Abs(value) > float.MaxValue)
                value = double.NaN;
            else
                value = (float)value;
        }
        if (!double.IsFinite(value) || value < minimum || value > maximum
            || exclusiveMinimum && value == minimum || integer && (value != Math.Truncate(value)
                || owner.ContainsKey(property) && !long.TryParse(owner[property]?.ToJsonString(), NumberStyles.AllowLeadingSign,
                    CultureInfo.InvariantCulture, out _)))
            errors.Add(path + "." + property + " is outside its supported range");
    }
}
