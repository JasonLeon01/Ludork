using Avalonia.Controls;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

internal sealed class MapFogEditor
{
    private readonly NumericUpDown density = EditorInputs.CreateNumericUpDown(0.5m, 0, 1, 0.05m);
    private readonly NumericUpDown scale = EditorInputs.CreateNumericUpDown(256, 1, 32768, 1);
    private readonly NumericUpDown velocityX = EditorInputs.CreateNumericUpDown(15, -9999, 9999, 1);
    private readonly NumericUpDown velocityY = EditorInputs.CreateNumericUpDown(15, -9999, 9999, 1);
    private readonly NumericUpDown turbulence = EditorInputs.CreateNumericUpDown(0.5m, 0, 1, 0.05m);
    private readonly NumericUpDown seed = EditorInputs.CreateNumericUpDown(0, 0, int.MaxValue, 1);
    private Color colour = Colors.White;

    public CheckBox Enabled { get; } = new();
    public TextBox PanoramaBox { get; } = EditorInputs.CreateReadOnlyTextBox();
    public StackPanel Options { get; } = new() { Spacing = 8 };
    public JsonObject Value => new()
    {
        ["enabled"] = Enabled.IsChecked == true,
        ["colour"] = new JsonArray(colour.R, colour.G, colour.B, colour.A),
        ["density"] = (double)(density.Value ?? 0.5m), ["scale"] = (double)(scale.Value ?? 256),
        ["velocity"] = new JsonArray((double)(velocityX.Value ?? 15), (double)(velocityY.Value ?? 15)),
        ["turbulence"] = (double)(turbulence.Value ?? 0.5m), ["seed"] = (int)(seed.Value ?? 0),
    };

    public MapFogEditor(Window owner, MapVisualSettings initial)
    {
        JsonObject fog = initial.Fog;
        Enabled.IsChecked = fog["enabled"]?.GetValue<bool>() == true;
        PanoramaBox.Text = initial.Panorama;
        density.Value = read(fog["density"], 0.5m);
        scale.Value = read(fog["scale"], 256);
        velocityX.Value = read(fog["velocity"]?[0], 15);
        velocityY.Value = read(fog["velocity"]?[1], 15);
        turbulence.Value = read(fog["turbulence"], 0.5m);
        seed.Value = read(fog["seed"], 0);
        if (fog["colour"] is JsonArray { Count: 4 } channels)
            colour = Color.FromArgb(channel(channels[3]), channel(channels[0]), channel(channels[1]), channel(channels[2]));
        BlueprintColourSwatch colourButton = new(colour);
        ToolTip.SetTip(colourButton, colour.ToString());
        colourButton.Click += async (_, _) =>
        {
            if (await MapColourPickerWindow.ShowAsync(owner, colour) is Color selected)
            {
                colour = selected;
                colourButton.Colour = colour;
                ToolTip.SetTip(colourButton, colour.ToString());
            }
        };
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_COLOUR"), colourButton));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_DENSITY"), density));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_SCALE"), scale));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_VELOCITY_X"), velocityX));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_VELOCITY_Y"), velocityY));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_TURBULENCE"), turbulence));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_SEED"), seed));
        Enabled.IsCheckedChanged += (_, _) => Options.IsVisible = Enabled.IsChecked == true;
        Options.IsVisible = Enabled.IsChecked == true;
    }

    private static decimal read(JsonNode? value, decimal fallback) => value is null ? fallback : JsonScalar.Decimal(value, (double)fallback);
    private static byte channel(JsonNode? value) => (byte)Math.Clamp(read(value, 255), 0, 255);
}
