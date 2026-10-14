using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class MaterialNumericEditor : StackPanel
{
    private readonly List<NumericUpDown> numbers = [];
    private long gesture;

    public MaterialNumericEditor(ProjectDataStore gameData, JsonObject descriptor, JsonNode? initial, Action<JsonNode?, bool> changed)
    {
        Spacing = 4;
        double? minimum = descriptor["min"] is JsonValue min ? JsonScalar.Number(min) : null;
        double? maximum = descriptor["max"] is JsonValue max ? JsonScalar.Number(max) : null;
        bool integer = descriptor["type"]?.GetValue<string>() == "integer";
        double step = descriptor["step"] is JsonValue increment ? JsonScalar.Number(increment) : integer ? 1 : 0.01;
        JsonArray? vector = initial?.DeepClone() as JsonArray;
        int count = vector?.Count ?? 1;
        for (int index = 0; index < count; index++)
        {
            int component = index;
            JsonNode? current = vector is null ? initial : vector[index];
            double currentNumber = JsonScalar.Number(current);
            TextBlock error = new()
            {
                Text = $"{LocaleService.Get("MATERIAL_OUTSIDE_RANGE")}: {current}",
                Foreground = Brushes.OrangeRed,
                TextWrapping = TextWrapping.Wrap,
                IsVisible = currentNumber < minimum || currentNumber > maximum,
            };
            void commit(JsonNode? next)
            {
                error.IsVisible = false;
                if (vector is null)
                    changed(next, false);
                else
                {
                    vector[component] = next?.DeepClone();
                    changed(vector.DeepClone(), false);
                }
            }
            Control editor;
            NumericUpDown number;
            if (descriptor["slider"]?.GetValue<bool>() != false
                && minimum.HasValue && maximum.HasValue && maximum > minimum && step > 0
                && (maximum.Value - minimum.Value) / step < int.MaxValue)
            {
                BlueprintProgressEditor progress = new(current, new BlueprintVariableRange(minimum.Value, maximum.Value, step), integer);
                progress.ValueChanged += (_, args) => commit(args.Value);
                progress.AddHandler(PointerPressedEvent, (_, args) =>
                {
                    if (args.Source is Visual source && (source is Slider || source.GetVisualAncestors().OfType<Slider>().Any()))
                        gesture = gameData.BeginHistoryGesture();
                }, RoutingStrategies.Tunnel);
                progress.AddHandler(PointerReleasedEvent, (_, _) => gameData.EndHistoryGesture(gesture), RoutingStrategies.Bubble, handledEventsToo: true);
                progress.PointerCaptureLost += (_, _) => gameData.EndHistoryGesture(gesture);
                number = progress.NumberInput;
                editor = progress;
            }
            else
            {
                number = EditorInputs.CreateNumericUpDown(toDecimal(JsonScalar.Number(current)),
                    minimum.HasValue ? toDecimal(minimum.Value) : decimal.MinValue,
                    maximum.HasValue ? toDecimal(maximum.Value) : decimal.MaxValue,
                    toDecimal(step));
                number.FormatString = integer ? "0" : "G";
                number.ValueChanged += (_, _) => commit(integer
                    ? JsonValue.Create(decimal.ToInt64(number.Value ?? 0))
                    : JsonValue.Create(decimal.ToDouble(number.Value ?? 0)));
                editor = number;
            }
            numbers.Add(number);
            if (vector is null)
                Children.Add(editor);
            else
            {
                Grid row = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*"), ColumnSpacing = 6 };
                row.Children.Add(new TextBlock { Text = (index + 1).ToString(), VerticalAlignment = VerticalAlignment.Center });
                Grid.SetColumn(editor, 1);
                row.Children.Add(editor);
                Children.Add(row);
            }
            Children.Add(error);
        }
        AttachedToVisualTree += (_, _) => numbers.ForEach(number => HistoryMergeBehavior.Attach(number, gameData));
        DetachedFromVisualTree += (_, _) =>
        {
            numbers.ForEach(HistoryMergeBehavior.Detach);
            gameData.EndHistoryGesture(gesture);
        };
    }

    private static decimal toDecimal(double number) => number >= (double)decimal.MaxValue ? decimal.MaxValue
        : number <= (double)decimal.MinValue ? decimal.MinValue : (decimal)number;
}
