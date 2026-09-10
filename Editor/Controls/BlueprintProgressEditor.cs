using Avalonia.Controls;
using Avalonia.Layout;
using Ludork.Views.Utils;
using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class BlueprintProgressEditor : Grid
{
    private readonly Slider slider = new();
    private readonly NumericUpDown number;
    private readonly BlueprintVariableRange range;
    private readonly bool returnInteger;
    private bool syncing;

    public BlueprintProgressEditor(JsonNode? value, BlueprintVariableRange valueRange, bool integer)
    {
        range = valueRange.Normalize();
        returnInteger = integer;
        int stepCount = Math.Max(1, (int)Math.Round((range.Maximum - range.Minimum) / range.Step));
        slider.Orientation = Orientation.Horizontal;
        slider.Minimum = 0;
        slider.Maximum = stepCount;
        slider.SmallChange = 1;
        slider.LargeChange = Math.Max(1, Math.Min(10, stepCount / 10));

        decimal minimum = toDecimal(range.Minimum);
        decimal maximum = toDecimal(range.Maximum);
        decimal increment = Math.Max(0.000001m, toDecimal(range.Step));
        number = EditorInputs.CreateNumericUpDown(0, minimum, maximum, increment, stretch: false);
        number.Width = 96;
        number.FormatString = createFormatString(range.Step);

        ColumnDefinitions = new ColumnDefinitions("*,96");
        ColumnSpacing = 6;
        Children.Add(slider);
        Grid.SetColumn(number, 1);
        Children.Add(number);

        slider.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setValue(range.Minimum + slider.Value * range.Step, true);
        };
        number.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setValue(decimal.ToDouble(number.Value ?? 0), true);
        };
        setValue(readDouble(value, range.Minimum), false);
    }

    public event EventHandler<BlueprintProgressValueChangedEventArgs>? ValueChanged;
    public NumericUpDown NumberInput => number;

    private void setValue(double value, bool emit)
    {
        double clamped = Math.Clamp(value, range.Minimum, range.Maximum);
        int sliderValue = Math.Clamp(
            (int)Math.Round((clamped - range.Minimum) / range.Step),
            0,
            (int)slider.Maximum);
        syncing = true;
        slider.Value = sliderValue;
        number.Value = toDecimal(clamped);
        syncing = false;
        if (!emit)
            return;
        JsonNode? result = returnInteger
            ? JsonValue.Create((int)Math.Round(clamped))
            : JsonValue.Create(roundToStep(clamped, range.Step));
        ValueChanged?.Invoke(this, new BlueprintProgressValueChangedEventArgs(result));
    }

    private static string createFormatString(double step)
    {
        string text = step.ToString("0.########", CultureInfo.InvariantCulture);
        int separator = text.IndexOf('.');
        if (separator < 0)
            return "0";
        int decimals = Math.Min(6, text.Length - separator - 1);
        return decimals <= 0 ? "0" : $"0.{new string('#', decimals)}";
    }

    private static double roundToStep(double value, double step)
    {
        string text = step.ToString("0.########", CultureInfo.InvariantCulture);
        int separator = text.IndexOf('.');
        int decimals = separator < 0 ? 0 : Math.Min(6, text.Length - separator - 1);
        return Math.Round(value, decimals);
    }

    private static double readDouble(JsonNode? value, double fallback)
    {
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out double number))
                return number;
            if (json.TryGetValue(out int integer))
                return integer;
            if (json.TryGetValue(out decimal decimalValue))
                return decimal.ToDouble(decimalValue);
        }
        return double.TryParse(value?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double parsed)
            ? parsed
            : fallback;
    }

    private static decimal toDecimal(double value)
    {
        if (value >= (double)decimal.MaxValue)
            return decimal.MaxValue;
        if (value <= (double)decimal.MinValue)
            return decimal.MinValue;
        return (decimal)value;
    }
}
