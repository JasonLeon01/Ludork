using Avalonia.Controls;
using Ludork.Models;
using Ludork.Services;

namespace Ludork.Views.Utils;

internal sealed class MapFogEditor
{
    private readonly MapVisualSettings initial;
    private readonly decimal? displayedOx;
    private readonly decimal? displayedOy;
    private readonly NumericUpDown power = EditorInputs.CreateNumericUpDown(0, 0, 100, 1);
    private readonly NumericUpDown ox = EditorInputs.CreateNumericUpDown(0, -9999, 9999, 1);
    private readonly NumericUpDown oy = EditorInputs.CreateNumericUpDown(0, -9999, 9999, 1);
    private readonly NumericUpDown distort = EditorInputs.CreateNumericUpDown(0, 0, 100, 1);

    public TextBox PathBox { get; } = EditorInputs.CreateReadOnlyTextBox();
    public TextBox PanoramaBox { get; } = EditorInputs.CreateReadOnlyTextBox();
    public StackPanel Options { get; } = new() { Spacing = 8 };
    public int Power => decimal.ToInt32(power.Value ?? 0);
    public double Ox => ox.Value == displayedOx ? initial.FogOx : (double)(ox.Value ?? 0);
    public double Oy => oy.Value == displayedOy ? initial.FogOy : (double)(oy.Value ?? 0);
    public int Distort => decimal.ToInt32(distort.Value ?? 0);

    public MapFogEditor(MapVisualSettings initial)
    {
        this.initial = initial;
        PathBox.Text = initial.Fog;
        PanoramaBox.Text = initial.Panorama;
        power.Value = initial.FogPower;
        ox.Value = JsonScalar.ToDecimal(initial.FogOx);
        oy.Value = JsonScalar.ToDecimal(initial.FogOy);
        displayedOx = ox.Value;
        displayedOy = oy.Value;
        distort.Value = initial.FogDistort;
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_POWER"), power));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_OX"), ox));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_OY"), oy));
        Options.Children.Add(EditorFormRows.Create(LocaleService.Get("MAP_FOG_DISTORT"), distort));
        PathBox.TextChanged += (_, _) => updateVisibility();
        updateVisibility();
    }

    private void updateVisibility() => Options.IsVisible = !string.IsNullOrWhiteSpace(PathBox.Text);
}
