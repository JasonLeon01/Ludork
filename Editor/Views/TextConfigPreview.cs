using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

internal sealed class TextConfigPreview : Border
{
    private readonly GameDataService gameData;
    private readonly Grid layers = new();

    public TextConfigPreview(GameDataService gameData)
    {
        this.gameData = gameData;
        Background = new SolidColorBrush(Color.Parse("#181818"));
        BorderBrush = new SolidColorBrush(Color.Parse("#3a3a3a"));
        BorderThickness = new Thickness(1);
        CornerRadius = new CornerRadius(4);
        Padding = new Thickness(44);
        ClipToBounds = true;
        Child = layers;
    }

    public void Update(JsonObject data, string text)
    {
        layers.Children.Clear();
        PreviewStyle style = PreviewStyle.FromConfig(data);
        IReadOnlyList<PreviewStyle> textStyles = activeStyles(data, text);
        JsonObject glow = objectValue(data["glow"]);
        if (boolValue(glow["enabled"]))
            addGlowLayers(data, text, style, glow);
        foreach (PreviewStyle outlineStyle in textStyles
                     .Where(item => item.OutlineThickness > 0)
                     .DistinctBy(item => item.OutlineThickness))
        {
            addOutlineLayers(data, text, outlineStyle);
        }
        layers.Children.Add(createText(
            data,
            text,
            new SolidColorBrush(style.FillColor),
            new Vector(0, 0)));
    }

    private void addGlowLayers(
        JsonObject data,
        string text,
        PreviewStyle style,
        JsonObject glow)
    {
        Color colour = colourValue(glow["color"], Colors.Transparent);
        double radius = Math.Max(0, numberValue(glow["radius"]));
        double intensity = Math.Clamp(numberValue(glow["intensity"]), 0, 1);
        if (radius <= 0 || intensity <= 0 || colour.A == 0)
            return;
        int rings = Math.Clamp((int)Math.Ceiling(radius / 2), 1, 4);
        for (int ring = rings; ring >= 1; ring -= 1)
        {
            double ringRadius = radius * ring / rings;
            double opacity = Math.Min(1, intensity / rings * 0.45);
            for (int index = 0; index < 16; index += 1)
            {
                double angle = Math.PI * 2 * index / 16;
                Control layer = createText(
                    data,
                    text,
                    new SolidColorBrush(colour),
                    new Vector(Math.Cos(angle) * ringRadius, Math.Sin(angle) * ringRadius),
                    PreviewLayer.Forced);
                layer.Opacity = opacity;
                layers.Children.Add(layer);
            }
        }
    }

    private void addOutlineLayers(
        JsonObject data,
        string text,
        PreviewStyle style)
    {
        int samples = Math.Clamp((int)Math.Ceiling(style.OutlineThickness * 8), 8, 32);
        SolidColorBrush brush = new(style.OutlineColor);
        for (int index = 0; index < samples; index += 1)
        {
            double angle = Math.PI * 2 * index / samples;
            layers.Children.Add(createText(
                data,
                text,
                brush,
                new Vector(
                    Math.Cos(angle) * style.OutlineThickness,
                    Math.Sin(angle) * style.OutlineThickness),
                stringValue(data["type"]) == "richTextConfig"
                    ? PreviewLayer.Outline
                    : PreviewLayer.Forced,
                style.OutlineThickness));
        }
    }

    private Control createText(
        JsonObject data,
        string text,
        IBrush brush,
        Vector offset,
        PreviewLayer layer = PreviewLayer.Fill,
        double targetOutlineThickness = 0)
    {
        return stringValue(data["type"]) == "richTextConfig"
            ? createRichText(
                data,
                text,
                brush,
                offset,
                layer,
                targetOutlineThickness)
            : createPlainText(data, text, brush, offset, layer);
    }

    private Control createPlainText(
        JsonObject data,
        string text,
        IBrush brush,
        Vector offset,
        PreviewLayer layer)
    {
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        FontFamily fontFamily = resolveFont(data);
        double measuredLineHeight = lineHeight(fontFamily, defaultStyle);
        double slantAngle = defaultStyle.Italic
            ? 0
            : Math.Clamp(numberValue(data["slantAngle"]), -45, 45);
        string normalizedText = text
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n');
        string[] lines = normalizedText.Split('\n');
        List<TextBlock> blocks = [];
        double maximumWidth = 1;
        foreach (string line in lines)
        {
            TextBlock block = new()
            {
                MaxWidth = 620,
                Height = measuredLineHeight,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                TextWrapping = TextWrapping.NoWrap,
                TextAlignment = alignment(stringValue(data["lineAlignment"])),
                FontFamily = fontFamily,
                FontSize = defaultStyle.CharacterSize,
                FontWeight = defaultStyle.Bold ? FontWeight.Bold : FontWeight.Normal,
                FontStyle = defaultStyle.Italic ? FontStyle.Italic : FontStyle.Normal,
                LetterSpacing = defaultStyle.LetterSpacing,
                LineHeight = measuredLineHeight,
                Foreground = brush,
                Text = line.Length == 0 ? "\u200B" : line,
                RenderTransform = new SkewTransform(-slantAngle, 0),
            };
            block.TextDecorations = decorations(defaultStyle);
            block.Measure(new Size(10000, measuredLineHeight));
            maximumWidth = Math.Max(
                maximumWidth,
                Math.Min(620, block.DesiredSize.Width));
            blocks.Add(block);
        }
        double totalHeight = Math.Max(1, measuredLineHeight * blocks.Count);
        StackPanel result = new()
        {
            Width = maximumWidth,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            RenderTransform = new TranslateTransform(offset.X, offset.Y),
            Spacing = 0,
        };
        double verticalOffset = 0;
        foreach (TextBlock block in blocks)
        {
            block.Width = maximumWidth;
            if (layer == PreviewLayer.Fill)
            {
                block.Foreground = createFillBrush(
                    data,
                    defaultStyle.FillColor,
                    maximumWidth,
                    measuredLineHeight,
                    verticalOffset / totalHeight,
                    measuredLineHeight / totalHeight,
                    true);
            }
            result.Children.Add(block);
            verticalOffset += measuredLineHeight;
        }
        return result;
    }

    private Control createRichText(
        JsonObject data,
        string text,
        IBrush layerBrush,
        Vector offset,
        PreviewLayer layer,
        double targetOutlineThickness)
    {
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        FontFamily fontFamily = resolveFont(data);
        IReadOnlyList<RichPreviewLine> lines = parseRichLines(data, text);
        List<RichLineVisual> lineVisuals = [];
        double maximumWidth = 1;
        foreach (RichPreviewLine line in lines)
        {
            IReadOnlyList<PreviewStyle> lineStyles = line.Segments.Count == 0
                ? [line.EndStyle]
                : line.Segments.Select(item => item.Style).Append(line.EndStyle).ToArray();
            double lineHeight = lineStyles.Max(item => TextConfigPreview.lineHeight(fontFamily, item));
            TextBlock block = new()
            {
                MaxWidth = 620,
                Height = lineHeight,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                TextWrapping = TextWrapping.NoWrap,
                TextAlignment = alignment(stringValue(data["lineAlignment"])),
                FontFamily = fontFamily,
                FontSize = line.EndStyle.CharacterSize,
                FontWeight = line.EndStyle.Bold ? FontWeight.Bold : FontWeight.Normal,
                FontStyle = line.EndStyle.Italic ? FontStyle.Italic : FontStyle.Normal,
                LetterSpacing = line.EndStyle.LetterSpacing,
                LineHeight = lineHeight,
                Foreground = layerBrush,
                TextDecorations = decorations(line.EndStyle),
            };
            List<RichRunVisual> runs = [];
            foreach (RichPreviewSegment segment in line.Segments)
            {
                Run run = createRun(
                    segment.Content,
                    segment.Style,
                    layerBrush,
                    layer,
                    targetOutlineThickness);
                block.Inlines!.Add(run);
                runs.Add(new RichRunVisual(run, segment.Style));
            }
            if (line.Segments.Count == 0)
            {
                Run empty = createRun(
                    "\u200B",
                    line.EndStyle,
                    layerBrush,
                    layer,
                    targetOutlineThickness);
                block.Inlines!.Add(empty);
                runs.Add(new RichRunVisual(empty, line.EndStyle));
            }
            block.Measure(new Size(10000, lineHeight));
            double desiredWidth = Math.Max(1, Math.Min(620, block.DesiredSize.Width));
            maximumWidth = Math.Max(maximumWidth, desiredWidth);
            lineVisuals.Add(new RichLineVisual(block, runs, lineHeight));
        }
        double totalHeight = Math.Max(1, lineVisuals.Sum(item => item.Height));
        StackPanel result = new()
        {
            Width = maximumWidth,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            RenderTransform = new TranslateTransform(offset.X, offset.Y),
            Spacing = 0,
        };
        double verticalOffset = 0;
        foreach (RichLineVisual visual in lineVisuals)
        {
            visual.Block.Width = maximumWidth;
            if (layer == PreviewLayer.Fill)
            {
                foreach (RichRunVisual run in visual.Runs)
                {
                    run.Run.Foreground = createFillBrush(
                        data,
                        run.Style.FillColor,
                        maximumWidth,
                        visual.Height,
                        verticalOffset / totalHeight,
                        visual.Height / totalHeight);
                }
            }
            result.Children.Add(visual.Block);
            verticalOffset += visual.Height;
        }
        return result;
    }

    private static double lineHeight(FontFamily fontFamily, PreviewStyle style)
    {
        Typeface typeface = new(
            fontFamily,
            style.Italic ? FontStyle.Italic : FontStyle.Normal,
            style.Bold ? FontWeight.Bold : FontWeight.Normal,
            FontStretch.Normal);
        if (!FontManager.Current.TryGetGlyphTypeface(typeface, out GlyphTypeface? glyphTypeface)
            || glyphTypeface is null)
        {
            return Math.Max(1, style.CharacterSize * style.LineSpacing);
        }
        FontMetrics metrics = glyphTypeface.Metrics;
        double naturalLineHeight = metrics.LineSpacing
            * style.CharacterSize
            / metrics.DesignEmHeight;
        return Math.Max(1, naturalLineHeight * style.LineSpacing);
    }

    private static Run createRun(
        string content,
        PreviewStyle style,
        IBrush layerBrush,
        PreviewLayer layer,
        double targetOutlineThickness)
    {
        IBrush fill;
        if (layer == PreviewLayer.Forced)
        {
            fill = layerBrush;
        }
        else if (layer == PreviewLayer.Outline)
        {
            fill = Math.Abs(style.OutlineThickness - targetOutlineThickness) < 0.001
                ? new SolidColorBrush(style.OutlineColor)
                : Brushes.Transparent;
        }
        else
        {
            fill = new SolidColorBrush(style.FillColor);
        }
        return new Run(content)
        {
            FontSize = style.CharacterSize,
            FontWeight = style.Bold ? FontWeight.Bold : FontWeight.Normal,
            FontStyle = style.Italic ? FontStyle.Italic : FontStyle.Normal,
            Foreground = fill,
            LetterSpacing = style.LetterSpacing,
            TextDecorations = decorations(style),
        };
    }

    private static IReadOnlyList<PreviewStyle> activeStyles(
        JsonObject data,
        string text)
    {
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        if (stringValue(data["type"]) != "richTextConfig")
            return [defaultStyle];
        List<PreviewStyle> result = parseRichLines(data, text)
            .SelectMany(line => line.Segments.Select(segment => segment.Style))
            .ToList();
        if (result.Count == 0)
            result.Add(defaultStyle);
        return result;
    }

    private static IReadOnlyList<RichPreviewLine> parseRichLines(
        JsonObject data,
        string text)
    {
        JsonObject styles = objectValue(data["styles"]);
        PreviewStyle defaultStyle = PreviewStyle.FromConfig(data);
        PreviewStyle current = defaultStyle;
        List<RichPreviewLine> result = [];
        List<RichPreviewSegment> segments = [];

        void appendContent(string content)
        {
            int offset = 0;
            while (offset <= content.Length)
            {
                int lineEnd = content.IndexOf('\n', offset);
                if (lineEnd < 0)
                {
                    string remaining = content[offset..];
                    if (remaining.Length != 0)
                        segments.Add(new RichPreviewSegment(remaining, current));
                    return;
                }
                string lineContent = content[offset..lineEnd];
                if (lineContent.Length != 0)
                    segments.Add(new RichPreviewSegment(lineContent, current));
                result.Add(new RichPreviewLine(segments, current));
                segments = [];
                offset = lineEnd + 1;
            }
        }

        foreach ((string content, string? marker) in parseSegments(text))
        {
            if (marker is null)
            {
                appendContent(content);
                continue;
            }
            if (marker == "default")
                current = defaultStyle;
            else if (styles[marker] is JsonObject named)
                current = current.Adapt(named);
            else if (tryParseMarkerColour(marker, out Color markerColour))
                current = current with { FillColor = markerColour };
            else
                appendContent($"#{marker}#");
        }
        result.Add(new RichPreviewLine(segments, current));
        if (text.EndsWith('\n') && result.Count > 1)
            result.RemoveAt(result.Count - 1);
        return result;
    }

    private static IEnumerable<(string Content, string? Marker)> parseSegments(string text)
    {
        int offset = 0;
        while (offset < text.Length)
        {
            int markerStart = text.IndexOf('#', offset);
            if (markerStart < 0)
            {
                yield return (text[offset..], null);
                yield break;
            }
            if (markerStart > offset)
                yield return (text[offset..markerStart], null);
            int markerEnd = text.IndexOf('#', markerStart + 1);
            if (markerEnd < 0)
            {
                yield return (text[markerStart..], null);
                yield break;
            }
            yield return (string.Empty, text[(markerStart + 1)..markerEnd]);
            offset = markerEnd + 1;
        }
    }

    private IBrush createFillBrush(
        JsonObject data,
        Color fallback,
        double width,
        double height,
        double verticalStart,
        double verticalSpan,
        bool useRelativeVerticalBounds = false)
    {
        JsonObject gradient = objectValue(data["gradient"]);
        if (!boolValue(gradient["enabled"]))
            return new SolidColorBrush(fallback);
        string curve = stringValue(gradient["curve"]);
        JsonObject? curveData = gameData.CurvesData.TryGetValue(curve, out JsonObject? value)
            ? value
            : null;
        PreviewVectorCurve? vectorCurve = createPreviewVectorCurve(curveData);
        if (vectorCurve is null)
            return new SolidColorBrush(fallback);
        bool horizontal = stringValue(gradient["direction"]) == "horizontal";
        LinearGradientBrush brush = new()
        {
            StartPoint = new RelativePoint(
                0,
                0,
                !horizontal && useRelativeVerticalBounds
                    ? RelativeUnit.Relative
                    : RelativeUnit.Absolute),
            EndPoint = horizontal
                ? new RelativePoint(Math.Max(1, width), 0, RelativeUnit.Absolute)
                : useRelativeVerticalBounds
                    ? new RelativePoint(0, 1, RelativeUnit.Relative)
                    : new RelativePoint(0, Math.Max(1, height), RelativeUnit.Absolute),
        };
        for (int index = 0; index <= 24; index += 1)
        {
            double position = index / 24.0;
            double curvePosition = horizontal
                ? position
                : verticalStart + position * verticalSpan;
            Color sampledColor = curveColour(evaluateCurve(vectorCurve, curvePosition));
            brush.GradientStops.Add(new GradientStop(multiply(fallback, sampledColor), position));
        }
        return brush;
    }

    private FontFamily resolveFont(JsonObject data)
    {
        string font = stringValue(data["font"]);
        return font.Length != 0
            && TextConfigFontLoader.TryResolve(gameData.ProjectPath, font, out FontFamily family)
                ? family
                : FontFamily.Default;
    }

    private static TextDecorationCollection? decorations(PreviewStyle style)
    {
        TextDecorationCollection result = new();
        if (style.Underlined)
        {
            foreach (TextDecoration decoration in TextDecorations.Underline)
                result.Add(decoration);
        }
        if (style.StrikeThrough)
        {
            foreach (TextDecoration decoration in TextDecorations.Strikethrough)
                result.Add(decoration);
        }
        return result.Count == 0 ? null : result;
    }

    private static TextAlignment alignment(string value)
    {
        return value switch
        {
            "center" => TextAlignment.Center,
            "right" => TextAlignment.Right,
            _ => TextAlignment.Left,
        };
    }

    private static bool tryParseMarkerColour(string marker, out Color colour)
    {
        string value = marker.TrimStart('#', '$');
        if (value.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            value = value[2..];
        if (value.Length is not 6 and not 8
            || !uint.TryParse(value, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint parsed))
        {
            colour = Colors.White;
            return false;
        }
        if (value.Length == 6)
            parsed = parsed << 8 | 255;
        colour = Color.FromArgb(
            (byte)(parsed & 255),
            (byte)(parsed >> 24),
            (byte)(parsed >> 16),
            (byte)(parsed >> 8));
        return true;
    }

    private static Color multiply(Color left, Color right)
    {
        return Color.FromArgb(
            multiplyChannel(left.A, right.A),
            multiplyChannel(left.R, right.R),
            multiplyChannel(left.G, right.G),
            multiplyChannel(left.B, right.B));
    }

    private static byte multiplyChannel(byte left, byte right)
    {
        return (byte)((left * right + 127) / 255);
    }

    private static Color curveColour(IReadOnlyList<double> value)
    {
        return Color.FromArgb(
            curveChannel(value[3]),
            curveChannel(value[0]),
            curveChannel(value[1]),
            curveChannel(value[2]));
    }

    private static byte curveChannel(double value)
    {
        return (byte)Math.Clamp((int)Math.Round(value), 0, 255);
    }

    private static PreviewVectorCurve? createPreviewVectorCurve(JsonObject? curve)
    {
        if (stringValue(curve?["type"]) != "vector4Curve"
            || curve?["keys"] is not JsonArray source)
        {
            return null;
        }
        List<PreviewVectorCurveKey> keys = source
            .OfType<JsonObject>()
            .Select(item => new PreviewVectorCurveKey(
                numberValue(item["time"]),
                vectorValue(item["value"], 4),
                stringValue(item["interpolation"]),
                vectorValue(item["arriveTangent"], 4),
                vectorValue(item["leaveTangent"], 4)))
            .OrderBy(item => item.Time)
            .ToList();
        return new PreviewVectorCurve(
            vectorValue(curve["defaultValue"], 4),
            keys);
    }

    private static double[] vectorValue(JsonNode? node, int componentCount)
    {
        JsonArray? values = node as JsonArray;
        double[] result = new double[componentCount];
        for (int index = 0; index < componentCount; index += 1)
        {
            result[index] = values is not null && index < values.Count
                ? numberValue(values[index])
                : 0;
        }
        return result;
    }

    private static double[] evaluateCurve(PreviewVectorCurve curve, double time)
    {
        IReadOnlyList<PreviewVectorCurveKey> keys = curve.Keys;
        if (keys.Count == 0)
            return curve.DefaultValue;
        if (time <= keys[0].Time)
            return keys[0].Value;
        if (time >= keys[^1].Time)
            return keys[^1].Value;
        for (int index = 0; index < keys.Count - 1; index += 1)
        {
            PreviewVectorCurveKey left = keys[index];
            PreviewVectorCurveKey right = keys[index + 1];
            if (time > right.Time)
                continue;
            double duration = Math.Max(0.000001, right.Time - left.Time);
            double position = (time - left.Time) / duration;
            if (left.Interpolation == "constant")
                return left.Value;
            double[] result = new double[left.Value.Length];
            if (left.Interpolation != "cubic")
            {
                for (int component = 0; component < result.Length; component += 1)
                {
                    result[component] = left.Value[component]
                        + (right.Value[component] - left.Value[component]) * position;
                }
                return result;
            }
            double position2 = position * position;
            double position3 = position2 * position;
            double h00 = 2 * position3 - 3 * position2 + 1;
            double h10 = position3 - 2 * position2 + position;
            double h01 = -2 * position3 + 3 * position2;
            double h11 = position3 - position2;
            for (int component = 0; component < result.Length; component += 1)
            {
                result[component] = h00 * left.Value[component]
                    + h10 * duration * left.LeaveTangent[component]
                    + h01 * right.Value[component]
                    + h11 * duration * right.ArriveTangent[component];
            }
            return result;
        }
        return keys[^1].Value;
    }

    private static JsonObject objectValue(JsonNode? node)
    {
        return node as JsonObject ?? new JsonObject();
    }

    private static string stringValue(JsonNode? node)
    {
        return node is JsonValue value && value.TryGetValue<string>(out string? result)
            ? result ?? string.Empty
            : string.Empty;
    }

    private static bool boolValue(JsonNode? node)
    {
        return node is JsonValue value && value.TryGetValue<bool>(out bool result) && result;
    }

    private static double numberValue(JsonNode? node, double fallback = 0)
    {
        return node is JsonValue value && value.TryGetValue<double>(out double result)
            ? result
            : fallback;
    }

    private static Color colourValue(JsonNode? node, Color fallback)
    {
        if (node is not JsonArray values || values.Count < 4)
            return fallback;
        return Color.FromArgb(
            byteValue(values[3], fallback.A),
            byteValue(values[0], fallback.R),
            byteValue(values[1], fallback.G),
            byteValue(values[2], fallback.B));
    }

    private static byte byteValue(JsonNode? node, byte fallback)
    {
        if (node is not JsonValue value || !value.TryGetValue<int>(out int result))
            return fallback;
        return (byte)Math.Clamp(result, 0, 255);
    }

    private sealed record PreviewVectorCurve(
        double[] DefaultValue,
        IReadOnlyList<PreviewVectorCurveKey> Keys);

    private sealed record PreviewVectorCurveKey(
        double Time,
        double[] Value,
        string Interpolation,
        double[] ArriveTangent,
        double[] LeaveTangent);

    private sealed record RichPreviewSegment(string Content, PreviewStyle Style);

    private sealed record RichPreviewLine(
        IReadOnlyList<RichPreviewSegment> Segments,
        PreviewStyle EndStyle);

    private sealed record RichRunVisual(Run Run, PreviewStyle Style);

    private sealed record RichLineVisual(
        TextBlock Block,
        IReadOnlyList<RichRunVisual> Runs,
        double Height);

    private enum PreviewLayer
    {
        Fill,
        Forced,
        Outline,
    }

    private sealed record PreviewStyle(
        double CharacterSize,
        bool Bold,
        bool Italic,
        bool Underlined,
        bool StrikeThrough,
        Color FillColor,
        double LetterSpacing,
        double LineSpacing,
        Color OutlineColor,
        double OutlineThickness)
    {
        public static PreviewStyle FromConfig(JsonObject data)
        {
            JsonObject source = stringValue(data["type"]) == "richTextConfig"
                ? objectValue(data["defaultStyle"])
                : data;
            JsonObject flags = objectValue(source["style"]);
            JsonObject outline = objectValue(source["outline"]);
            return new PreviewStyle(
                Math.Max(1, numberValue(source["characterSize"], 22)),
                boolValue(flags["bold"]),
                boolValue(flags["italic"]),
                boolValue(flags["underlined"]),
                boolValue(flags["strikeThrough"]),
                colourValue(source["fillColor"], Colors.White),
                numberValue(source["letterSpacing"], 1),
                numberValue(source["lineSpacing"], 1),
                colourValue(outline["color"], Colors.Black),
                Math.Max(0, numberValue(outline["thickness"])));
        }

        public PreviewStyle Adapt(JsonObject partial)
        {
            JsonObject flags = objectValue(partial["style"]);
            JsonObject outline = objectValue(partial["outline"]);
            return this with
            {
                CharacterSize = partial.ContainsKey("characterSize")
                    ? Math.Max(1, numberValue(partial["characterSize"], CharacterSize))
                    : CharacterSize,
                Bold = flags.ContainsKey("bold") ? boolValue(flags["bold"]) : Bold,
                Italic = flags.ContainsKey("italic") ? boolValue(flags["italic"]) : Italic,
                Underlined = flags.ContainsKey("underlined") ? boolValue(flags["underlined"]) : Underlined,
                StrikeThrough = flags.ContainsKey("strikeThrough")
                    ? boolValue(flags["strikeThrough"])
                    : StrikeThrough,
                FillColor = partial.ContainsKey("fillColor")
                    ? colourValue(partial["fillColor"], FillColor)
                    : FillColor,
                LetterSpacing = partial.ContainsKey("letterSpacing")
                    ? numberValue(partial["letterSpacing"], LetterSpacing)
                    : LetterSpacing,
                LineSpacing = partial.ContainsKey("lineSpacing")
                    ? numberValue(partial["lineSpacing"], LineSpacing)
                    : LineSpacing,
                OutlineColor = outline.ContainsKey("color")
                    ? colourValue(outline["color"], OutlineColor)
                    : OutlineColor,
                OutlineThickness = outline.ContainsKey("thickness")
                    ? Math.Max(0, numberValue(outline["thickness"], OutlineThickness))
                    : OutlineThickness,
            };
        }
    }
}
