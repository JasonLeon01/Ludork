using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class LightInfoPanel : UserControl
{
    private readonly StackPanel form = new() { Spacing = 8 };
    private readonly TextBox[] positionEditors = [createEditor(), createEditor()];
    private readonly TextBox[] colorEditors = [createEditor(), createEditor(), createEditor(), createEditor()];
    private readonly TextBox radiusEditor = createEditor();
    private readonly TextBox intensityEditor = createEditor();
    private readonly Border colorPreview = new() { Width = 28, Height = 28, BorderBrush = Brushes.DimGray, BorderThickness = new Thickness(1) };
    private bool isLoading;
    private JsonObject? lightData;

    public LightInfoPanel()
    {
        foreach (TextBox editor in positionEditors)
            connectEditor(editor);
        foreach (TextBox editor in colorEditors)
            connectEditor(editor);
        connectEditor(radiusEditor);
        connectEditor(intensityEditor);
        Content = new StackPanel
        {
            Margin = new Thickness(8),
            Spacing = 8,
            Children = { form, new Border { VerticalAlignment = Avalonia.Layout.VerticalAlignment.Stretch } },
        };
        form.Children.Add(createRow("position", createEditorRow(positionEditors)));
        form.Children.Add(createRow("color", createColorEditorRow()));
        form.Children.Add(createRow("radius", radiusEditor));
        form.Children.Add(createRow("intensity", intensityEditor));
        setLight(null);
    }

    public event EventHandler<LightInfoEditedEventArgs>? LightEdited;

    public void setLight(JsonObject? nextLight)
    {
        lightData = nextLight;
        form.IsVisible = nextLight is not null;
        if (nextLight is null)
            return;
        updateLight(nextLight);
    }

    public void updateLight(JsonObject? nextLight)
    {
        if (nextLight is null)
        {
            setLight(null);
            return;
        }
        if (lightData is null)
        {
            setLight(nextLight);
            return;
        }
        lightData = nextLight;
        isLoading = true;
        setEditors(positionEditors, nextLight["position"] as JsonArray, 2);
        setEditors(colorEditors, nextLight["color"] as JsonArray, 4);
        radiusEditor.Text = formatNumber(getNumber(nextLight["radius"]));
        intensityEditor.Text = formatNumber(getNumber(nextLight["intensity"]));
        colorPreview.Background = new SolidColorBrush(Color.FromArgb(
            (byte)Math.Clamp((int)getNumber(nextLight["color"] is JsonArray color && color.Count > 3 ? color[3] : null, 255), 0, 255),
            (byte)Math.Clamp((int)getNumber(nextLight["color"] is JsonArray red && red.Count > 0 ? red[0] : null, 255), 0, 255),
            (byte)Math.Clamp((int)getNumber(nextLight["color"] is JsonArray green && green.Count > 1 ? green[1] : null, 255), 0, 255),
            (byte)Math.Clamp((int)getNumber(nextLight["color"] is JsonArray blue && blue.Count > 2 ? blue[2] : null, 255), 0, 255)));
        isLoading = false;
    }

    private void onEditorFinished(object? sender, EventArgs args)
    {
        if (isLoading || lightData is null)
            return;
        JsonObject next = new()
        {
            ["position"] = new JsonArray(parseNumber(positionEditors[0].Text), parseNumber(positionEditors[1].Text)),
            ["color"] = new JsonArray(parseNumber(colorEditors[0].Text), parseNumber(colorEditors[1].Text), parseNumber(colorEditors[2].Text), parseNumber(colorEditors[3].Text)),
            ["radius"] = parseNumber(radiusEditor.Text),
            ["intensity"] = parseNumber(intensityEditor.Text),
        };
        if (JsonNode.DeepEquals(lightData, next))
            return;
        lightData = next;
        LightEdited?.Invoke(this, new LightInfoEditedEventArgs(next));
    }

    private static TextBox createEditor()
    {
        TextBox box = EditorInputs.CreateEditableTextBox();
        box.MinWidth = 0;
        box.HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch;
        return box;
    }

    private void connectEditor(TextBox editor)
    {
        editor.LostFocus += onEditorFinished;
        editor.KeyDown += (_, args) =>
        {
            if (args.Key == Key.Enter)
            {
                onEditorFinished(editor, EventArgs.Empty);
                args.Handled = true;
            }
        };
    }

    private static Control createRow(string localeKey, Control editor)
    {
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 8 };
        row.Children.Add(new TextBlock { Text = LocaleService.Get(localeKey), VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center });
        Grid.SetColumn(editor, 1);
        row.Children.Add(editor);
        return row;
    }

    private static Control createEditorRow(TextBox[] editors)
    {
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("*,*"), ColumnSpacing = 6 };
        for (int index = 0; index < editors.Length; index++)
        {
            Grid.SetColumn(editors[index], index);
            row.Children.Add(editors[index]);
        }
        return row;
    }

    private Control createColorEditorRow()
    {
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*,*,*,*"), ColumnSpacing = 6 };
        row.Children.Add(colorPreview);
        for (int index = 0; index < colorEditors.Length; index++)
        {
            Grid.SetColumn(colorEditors[index], index + 1);
            row.Children.Add(colorEditors[index]);
        }
        return row;
    }

    private static void setEditors(TextBox[] editors, JsonArray? values, int count)
    {
        for (int index = 0; index < count; index++)
            editors[index].Text = formatNumber(getNumber(values is not null && index < values.Count ? values[index] : null));
    }

    private static double parseNumber(string? text)
    {
        return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : 0;
    }

    private static double getNumber(JsonNode? value, double fallback = 0)
    {
        return double.TryParse(value?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double number) ? number : fallback;
    }

    private static string formatNumber(double value)
    {
        return value % 1 == 0 ? ((int)value).ToString(CultureInfo.InvariantCulture) : value.ToString(CultureInfo.InvariantCulture);
    }
}

public sealed class LightInfoEditedEventArgs(JsonObject lightData) : EventArgs
{
    public JsonObject LightData { get; } = lightData;
}
