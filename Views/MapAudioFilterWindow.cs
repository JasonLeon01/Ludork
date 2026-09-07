using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed class MapAudioFilterWindow : Window
{
    private readonly bool isBgm;
    private readonly JsonObject initial;
    private readonly Dictionary<NumericUpDown, decimal?> displayedValues;
    private readonly NumericUpDown offsetBox;
    private readonly NumericUpDown pitchBox;
    private readonly NumericUpDown panBox;
    private readonly NumericUpDown volumeBox;
    private readonly NumericUpDown loopStartBox;
    private readonly NumericUpDown loopEndBox;

    private MapAudioFilterWindow(JsonObject initial, bool isBgm)
    {
        this.isBgm = isBgm;
        this.initial = (JsonObject)initial.DeepClone();
        Title = LocaleService.Get(isBgm ? "EDIT_BGM_FILTER" : "EDIT_BGS_FILTER");
        Width = isBgm ? 403 : 320;
        Height = isBgm ? 272 : 230;
        MinWidth = 320;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);
        offsetBox = createNumber(getValue(initial, "offset", 0), 0, 999999, 0.1m);
        pitchBox = createNumber(getValue(initial, "pitch", 1), 0.01m, 4, 0.05m);
        panBox = createNumber(getValue(initial, "pan", 0), -1, 1, 0.1m);
        volumeBox = createNumber(getValue(initial, "volume", 100), 0, 100, 1);
        JsonObject loopPoint = initial["loopPoint"] as JsonObject ?? new JsonObject();
        loopStartBox = createNumber(getValue(loopPoint, "start", 0), 0, 999999, 0.1m);
        loopEndBox = createNumber(getValue(loopPoint, "end", 0), 0, 999999, 0.1m);
        displayedValues = new Dictionary<NumericUpDown, decimal?>
        {
            [offsetBox] = offsetBox.Value,
            [pitchBox] = pitchBox.Value,
            [panBox] = panBox.Value,
            [volumeBox] = volumeBox.Value,
            [loopStartBox] = loopStartBox.Value,
            [loopEndBox] = loopEndBox.Value,
        };

        Grid form = new() { RowSpacing = 8 };
        addRow(form, LocaleService.Get("FILTER_OFFSET"), offsetBox);
        addRow(form, LocaleService.Get("FILTER_PITCH"), pitchBox);
        addRow(form, LocaleService.Get("FILTER_PAN"), panBox);
        addRow(form, LocaleService.Get("FILTER_VOLUME"), volumeBox);
        if (isBgm)
        {
            Grid loop = new() { ColumnDefinitions = new ColumnDefinitions("*,Auto,*"), ColumnSpacing = 6 };
            loop.Children.Add(loopStartBox);
            TextBlock divider = new() { Text = "/", VerticalAlignment = VerticalAlignment.Center };
            Grid.SetColumn(divider, 1);
            loop.Children.Add(divider);
            Grid.SetColumn(loopEndBox, 2);
            loop.Children.Add(loopEndBox);
            addRow(form, LocaleService.Get("FILTER_LOOP_POINT"), loop);
        }
        Button confirm = new() { Content = LocaleService.Get("CONFIRM") };
        confirm.Click += (_, _) => Close(buildResult());
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close(null);
        StackPanel content = new() { Margin = new Thickness(20), Spacing = 12 };
        content.Children.Add(form);
        content.Children.Add(new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Spacing = 8, Children = { confirm, cancel } });
        Content = content;
    }

    public static Task<JsonObject?> ShowAsync(Window owner, JsonObject initial, bool isBgm)
    {
        return new MapAudioFilterWindow(initial, isBgm).ShowDialog<JsonObject?>(owner);
    }

    private JsonObject buildResult()
    {
        JsonObject result = (JsonObject)initial.DeepClone();
        applyChangedValue(result, "offset", offsetBox, 0);
        applyChangedValue(result, "pitch", pitchBox, 1);
        applyChangedValue(result, "pan", panBox, 0);
        applyChangedValue(result, "volume", volumeBox, 100);
        if (isBgm && (isChanged(loopStartBox) || isChanged(loopEndBox)))
        {
            decimal loopStart = loopStartBox.Value ?? 0;
            decimal loopEnd = loopEndBox.Value ?? 0;
            if (loopStart == 0 && loopEnd == 0)
                result.Remove("loopPoint");
            else
            {
                JsonObject loopPoint = result["loopPoint"] as JsonObject ?? new JsonObject();
                if (isChanged(loopStartBox) || !loopPoint.ContainsKey("start"))
                    loopPoint["start"] = loopStart;
                if (isChanged(loopEndBox) || !loopPoint.ContainsKey("end"))
                    loopPoint["end"] = loopEnd;
                result["loopPoint"] = loopPoint;
            }
        }
        return result;
    }

    private static void addRow(Grid form, string label, Control editor)
    {
        int rowIndex = form.RowDefinitions.Count;
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("120,*"), ColumnSpacing = 8 };
        row.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap });
        Grid.SetColumn(editor, 1);
        row.Children.Add(editor);
        Grid.SetRow(row, rowIndex);
        form.Children.Add(row);
    }

    private static NumericUpDown createNumber(decimal value, decimal minimum, decimal maximum, decimal increment)
    {
        return EditorInputs.CreateNumericUpDown(value, minimum, maximum, increment);
    }

    private static decimal getValue(JsonObject values, string name, decimal fallback)
    {
        return values[name]?.GetValue<decimal?>() ?? fallback;
    }

    private bool isChanged(NumericUpDown number) => number.Value != displayedValues[number];

    private void applyChangedValue(JsonObject result, string key, NumericUpDown number, decimal defaultValue)
    {
        if (!isChanged(number))
            return;
        decimal value = number.Value ?? defaultValue;
        if (value == defaultValue)
            result.Remove(key);
        else
            result[key] = value;
    }
}
