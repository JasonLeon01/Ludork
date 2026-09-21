using Ludork.Models;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class ParticleCurveEditor : UserControl
{
    private static readonly string[] Channels = ["speed", "sizeX", "sizeY", "rotation", "red", "green", "blue", "alpha"];
    private readonly ProjectDataStore gameData;
    private readonly JsonObject track;
    private readonly Action commit;
    private readonly CurveCanvas canvas = new() { MinHeight = 170 };
    private readonly ComboBox channel = new() { Width = 120 };
    private readonly ComboBox resource = new() { Width = 170 };
    private readonly NumericUpDown keyTime = EditorInputs.CreateNumericUpDown(0, 0, 1, 0.01m);
    private readonly NumericUpDown keyValue = EditorInputs.CreateNumericUpDown(1, -10000, 10000, 0.05m);
    private readonly ComboBox interpolation = new() { ItemsSource = new[] { "constant", "linear", "cubic" }, Width = 90 };
    private readonly TextBlock state = new() { Foreground = EditorTheme.Brush("TextMuted") };
    private JsonObject curve = new();
    private string[] resourceKeys = [];
    private bool refreshing;
    private long gesture;

    public ParticleCurveEditor(ProjectDataStore gameData, JsonObject track, Action commit)
    {
        this.gameData = gameData;
        this.track = track;
        this.commit = commit;
        channel.ItemsSource = Channels.Select(name => LocaleService.Get("PARTICLE_CURVE_" + name.ToUpperInvariant())).ToArray();
        channel.SelectedIndex = 0;
        channel.SelectionChanged += (_, _) => load();
        resource.SelectionChanged += (_, _) =>
        {
            if (refreshing || resource.SelectedIndex < 0)
                return;
            string key = resourceKeys[resource.SelectedIndex];
            ensureCurves();
            if (key.Length == 0)
                curves.Remove(selectedChannel);
            else
                curves[selectedChannel] = key;
            commit();
            load();
        };
        WrapPanel toolbar = new() { Orientation = Orientation.Horizontal, Margin = new Thickness(6) };
        toolbar.Children.Add(channel);
        toolbar.Children.Add(resource);
        Button edit = new() { Content = LocaleService.Get("PARTICLE_EDIT_CURVE") };
        edit.Click += (_, _) =>
        {
            ensureCurves();
            curves[selectedChannel] = curve.DeepClone();
            commit();
            load();
        };
        toolbar.Children.Add(edit);
        Button fit = new() { Content = LocaleService.Get("PARTICLE_FIT_CURVE") };
        fit.Click += (_, _) => canvas.FitView();
        toolbar.Children.Add(fit);
        foreach (Control child in toolbar.Children)
            child.Margin = new Thickness(0, 0, 5, 4);
        StackPanel inspector = new() { Orientation = Orientation.Horizontal, Spacing = 6, Margin = new Thickness(6) };
        keyTime.Width = 84;
        keyValue.Width = 94;
        inspector.Children.Add(new TextBlock { Text = "t", VerticalAlignment = VerticalAlignment.Center });
        inspector.Children.Add(keyTime);
        inspector.Children.Add(keyValue);
        inspector.Children.Add(interpolation);
        inspector.Children.Add(state);
        HistoryMergeBehavior.Attach(keyTime, gameData);
        HistoryMergeBehavior.Attach(keyValue, gameData);
        keyTime.ValueChanged += (_, _) => updateKey();
        keyValue.ValueChanged += (_, _) => updateKey();
        interpolation.SelectionChanged += (_, _) => updateKey();
        canvas.DataChanged += onCanvasChanged;
        canvas.SelectionChanged += _ => selectKey();
        canvas.AddHandler(PointerPressedEvent, (_, _) => gesture = gameData.BeginHistoryGesture(), RoutingStrategies.Tunnel);
        canvas.AddHandler(PointerReleasedEvent, (_, _) => gameData.EndHistoryGesture(gesture), RoutingStrategies.Bubble);
        Grid root = new() { RowDefinitions = new RowDefinitions("Auto,*,Auto") };
        root.Children.Add(toolbar);
        Grid.SetRow(canvas, 1);
        root.Children.Add(canvas);
        Grid.SetRow(inspector, 2);
        root.Children.Add(inspector);
        Content = root;
        load();
    }

    private string selectedChannel => Channels[Math.Max(channel.SelectedIndex, 0)];
    private JsonObject curves => track["curves"] as JsonObject ?? new JsonObject();

    private void ensureCurves()
    {
        if (track["curves"] is not JsonObject)
            track["curves"] = new JsonObject();
    }

    private void load()
    {
        refreshing = true;
        JsonNode? entry = curves[selectedChannel];
        string? reference = entry is JsonValue value && value.TryGetValue<string>(out string? text) ? text : null;
        curve = entry is JsonObject inline ? (JsonObject)inline.DeepClone()
            : reference is not null && gameData.Assets.CurvesData.TryGetValue(reference, out CurveSnapshot? existing)
                ? existing.ToJson() : ParticleAssetSchema.CreateCurve(selectedChannel);
        resourceKeys = gameData.Assets.CurvesData.Where(pair => pair.Value.Type == "curve")
            .Select(pair => pair.Key).OrderBy(key => key, StringComparer.Ordinal).Prepend(string.Empty).ToArray();
        resource.ItemsSource = resourceKeys.Select(key => key.Length == 0 ? LocaleService.Get("PARTICLE_INLINE_CURVE") : key).ToArray();
        resource.SelectedIndex = Math.Max(0, Array.IndexOf(resourceKeys, reference ?? string.Empty));
        canvas.IsEnabled = reference is null;
        canvas.SetCurveData(curve["keys"] as JsonArray ?? new JsonArray(),
            [curve["defaultValue"]?.GetValue<double>() ?? (selectedChannel == "rotation" ? 0 : 1)], "constant", "constant", 1);
        canvas.FitView();
        state.Text = reference is null ? LocaleService.Get("PARTICLE_CURVE_HINT") : LocaleService.Get("PARTICLE_CURVE_REFERENCE");
        refreshing = false;
        selectKey();
    }

    private void selectKey()
    {
        refreshing = true;
        CurveKey? selected = canvas.SelectedKey;
        keyTime.IsEnabled = selected is not null && canvas.IsEnabled;
        keyValue.IsEnabled = keyTime.IsEnabled;
        interpolation.IsEnabled = keyTime.IsEnabled;
        if (selected is not null)
        {
            keyTime.Value = (decimal)Math.Clamp(selected.Time, 0, 1);
            keyValue.Value = (decimal)Math.Clamp(selected.Value[0], -10000, 10000);
            interpolation.SelectedItem = selected.Interpolation;
        }
        refreshing = false;
    }

    private void updateKey()
    {
        if (refreshing || canvas.SelectedKey is not CurveKey selected || keyTime.Value is not decimal time || keyValue.Value is not decimal value)
            return;
        canvas.UpdateSelectedKey((double)time, (double)value, interpolation.SelectedItem as string ?? "linear",
            selected.ArriveTangent[0], selected.LeaveTangent[0]);
    }

    private void onCanvasChanged()
    {
        JsonArray keys = new();
        bool normalized = false;
        foreach (JsonObject key in canvas.ExportKeys().OfType<JsonObject>())
        {
            double time = ParticleAssetSchema.Number(key["time"]);
            double clamped = Math.Clamp(time, 0, 1);
            normalized |= clamped != time;
            key["time"] = clamped;
            if (keys.OfType<JsonObject>().Any(existing => existing["time"]!.GetValue<double>() == key["time"]!.GetValue<double>()))
            {
                normalized = true;
                continue;
            }
            keys.Add(key.DeepClone());
        }
        curve["keys"] = keys;
        if (normalized)
            canvas.SetCurveData(keys, [ParticleAssetSchema.Number(curve["defaultValue"], selectedChannel == "rotation" ? 0 : 1)],
                "constant", "constant", 1);
        ensureCurves();
        curves[selectedChannel] = curve.DeepClone();
        commit();
        selectKey();
    }
}
