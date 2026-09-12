using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views;
using Ludork.Views.Utils;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class ParticleEditor
{
    private void buildProperties()
    {
        properties.Children.Clear();
        module("PARTICLE_SYSTEM", system =>
        {
            addText(system, data, "name");
            addNumber(system, data, "simulationRate", 60, 1, 240, 1);
            addNumber(system, data, "seed", 1, 0, 16777215, 1);
        }, true);
        if (track is not JsonObject current)
        {
            properties.Children.Add(new TextBlock { Text = LocaleService.Get("PARTICLE_NO_TRACK"), TextWrapping = TextWrapping.Wrap });
            curveHost.Content = null;
            return;
        }
        module("PARTICLE_MAIN", main =>
        {
            addText(main, current, "name");
            addBool(main, current, "enabled", true);
            addChoice(main, current, "mode", ["emission", "resident"]);
            addNumber(main, current, "duration", 2, 0.001, 3600);
            addNumber(main, current, "delay", 0, 0, 3600);
            addBool(main, current, "loop", true);
            addBool(main, current, "prewarm", false);
            addNumber(main, current, "capacity", 1024, 1, 1000000, 1);
            addVector(main, current, "lifetime", [1, 2], 0.001, 3600);
        }, true);
        module("PARTICLE_EMISSION", emission =>
        {
            if (text(current, "mode", "emission") == "resident")
                addNumber(emission, current, "count", 32, 0, number(current["capacity"], 1024), 1);
            else
            {
                addNumber(emission, current, "rate", 30, 0, 100000);
                addNumber(emission, current, "distanceRate", 0, 0, 100000);
                addBursts(emission, current);
            }
        }, true);
        module("PARTICLE_SHAPE", shape =>
        {
            addChoice(shape, current, "shape", ["point", "line", "rectangle", "disk", "ring"]);
            addVector(shape, current, "extent", [32, 32], 0);
            addNumber(shape, current, "radius", 16, 0);
            addNumber(shape, current, "innerRadius", 8, 0);
            addNumber(shape, current, "direction", -90, -360, 360);
            addNumber(shape, current, "spread", 30, 0, 360);
        }, true);
        module("PARTICLE_INITIAL", initial =>
        {
            addVector(initial, current, "speed", [20, 40]);
            addVector(initial, current, "sizeMin", [8, 8], 0);
            addVector(initial, current, "sizeMax", [16, 16], 0);
            addVector(initial, current, "rotation", [0, 360]);
            addVector(initial, current, "angularVelocity", [0, 0]);
            addColour(initial, current, "colourMin");
            addColour(initial, current, "colourMax");
        }, true);
        module("PARTICLE_FORCES", forces =>
        {
            addVector(forces, current, "gravity", [0, 0]);
            addNumber(forces, current, "radialAcceleration", 0);
            addNumber(forces, current, "tangentialAcceleration", 0);
            addNumber(forces, current, "damping", 0, 0);
        });
        module("PARTICLE_RENDERER", renderer =>
        {
            addTexture(renderer, current);
            addVector(renderer, current, "textureRect", [0, 0, 0, 0], 0, 16384, 1,
                [LocaleService.Get("PARTICLE_RECT_POSITION"), LocaleService.Get("PARTICLE_RECT_SIZE")]);
            addChoice(renderer, current, "blend", ["alpha", "add"]);
        }, true);
        module("PARTICLE_SHEET", sheet =>
        {
            addNumber(sheet, current, "columns", 1, 1, 4096, 1);
            addNumber(sheet, current, "rows", 1, 1, 4096, 1);
            addNumber(sheet, current, "frameCount", 1, 1, 16777216, 1);
            addNumber(sheet, current, "frameRate", 0, 0, 1000);
            addBool(sheet, current, "randomStartFrame", false);
            addBool(sheet, current, "frameLoop", true);
        });
        module("PARTICLE_TRANSFORM", transform =>
        {
            addChoice(transform, current, "space", ["local", "world"]);
            addChoice(transform, current, "scaleMode", ["hierarchy", "local", "shape"]);
            addVector(transform, current, "offset", [0, 0]);
            addNumber(transform, current, "rotationOffset", 0);
            addVector(transform, current, "scale", [1, 1]);
        });
        curveHost.Content = new ParticleCurveEditor(gameData, current, commit);
    }

    private void module(string label, Action<StackPanel> populate, bool expanded = false)
    {
        properties.Children.Add(createExpander(label, () =>
        {
            StackPanel fields = new() { Spacing = 6, Margin = new Thickness(6) };
            populate(fields);
            return fields;
        }, expanded));
    }

    private static Expander createExpander(string label, Func<Control> createContent, bool expanded = false)
    {
        Expander expander = new()
        {
            Header = LocaleService.Get(label), IsExpanded = expanded,
            HorizontalAlignment = HorizontalAlignment.Stretch, HorizontalContentAlignment = HorizontalAlignment.Stretch,
        };
        Control? content = null;
        bool attached = false;
        bool pending = false;
        void prepareContent()
        {
            if (!attached || !expander.IsExpanded || pending || content is not null)
                return;
            pending = true;
            Avalonia.Threading.Dispatcher.UIThread.Post(() =>
            {
                pending = false;
                if (!attached || !expander.IsExpanded)
                    return;
                content ??= createContent();
                expander.Content = content;
            }, Avalonia.Threading.DispatcherPriority.Background);
        }
        expander.AttachedToVisualTree += (_, _) =>
        {
            attached = true;
            prepareContent();
        };
        expander.DetachedFromVisualTree += (_, _) => attached = false;
        expander.PropertyChanged += (_, args) =>
        {
            if (args.Property != Expander.IsExpandedProperty)
                return;
            if (expander.IsExpanded)
            {
                expander.Content = content;
                prepareContent();
            }
            else
                expander.Content = null;
        };
        return expander;
    }

    private static void field(StackPanel parent, string key, Control input)
    {
        StackPanel row = new() { Spacing = 3 };
        row.Children.Add(new TextBlock { Text = LocaleService.Get("PARTICLE_FIELD_" + key.ToUpperInvariant()), Foreground = EditorTheme.Brush("TextMuted") });
        row.Children.Add(input);
        parent.Children.Add(row);
    }

    private void addText(StackPanel parent, JsonObject owner, string property)
    {
        TextBox box = EditorInputs.CreateEditableTextBox(text(owner, property));
        HistoryMergeBehavior.Attach(box, gameData);
        box.TextChanged += (_, _) =>
        {
            owner[property] = box.Text ?? string.Empty;
            commit();
        };
        box.LostFocus += (_, _) =>
        {
            if (property == "name" && ReferenceEquals(owner, track))
            {
                refreshing = true;
                trackList.ItemsSource = tracks.OfType<JsonObject>().Select(item =>
                    (item["enabled"]?.GetValue<bool>() == false ? "○ " : "● ") + text(item, "name")).ToArray();
                trackList.SelectedIndex = selectedTrack;
                refreshing = false;
            }
        };
        field(parent, property, box);
    }

    private void addNumber(StackPanel parent, JsonObject owner, string property, double fallback,
        double minimum = -1000000, double maximum = 1000000, double increment = 0.1)
    {
        NumericUpDown box = numeric(number(owner[property], fallback), minimum, maximum, increment);
        HistoryMergeBehavior.Attach(box, gameData);
        box.ValueChanged += (_, _) =>
        {
            if (box.Value is not decimal value)
                return;
            owner[property] = increment == 1 ? JsonValue.Create((int)value) : JsonValue.Create((double)value);
            if (property == "capacity" && number(owner["count"], 32) > (double)value)
                owner["count"] = (int)value;
            if (property is "columns" or "rows")
            {
                int frames = (int)(number(owner["columns"], 1) * number(owner["rows"], 1));
                if (number(owner["frameCount"], 1) > frames)
                    owner["frameCount"] = frames;
            }
            commit();
        };
        field(parent, property, box);
    }

    private void addVector(StackPanel parent, JsonObject owner, string property, double[] fallback,
        double minimum = -1000000, double maximum = 1000000, double increment = 0.1, string[]? rowLabels = null)
    {
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions(rowLabels is null ? "*,*" : "Auto,*,*"),
            RowDefinitions = new RowDefinitions(string.Join(',', Enumerable.Repeat("Auto", (fallback.Length + 1) / 2))),
            ColumnSpacing = 4,
            RowSpacing = 4,
        };
        if (rowLabels is not null)
        {
            for (int index = 0; index < rowLabels.Length; index++)
            {
                TextBlock label = new() { Text = rowLabels[index], VerticalAlignment = VerticalAlignment.Center };
                Grid.SetRow(label, index);
                row.Children.Add(label);
            }
        }
        JsonArray source = owner[property] as JsonArray ?? ParticleAssetSchema.Array(fallback);
        for (int index = 0; index < fallback.Length; index++)
        {
            int component = index;
            NumericUpDown box = numeric(index < source.Count ? number(source[index], fallback[index]) : fallback[index], minimum, maximum, increment);
            HistoryMergeBehavior.Attach(box, gameData);
            box.ValueChanged += (_, _) =>
            {
                if (box.Value is not decimal value)
                    return;
                JsonArray values = owner[property] as JsonArray ?? ParticleAssetSchema.Array(fallback);
                values[component] = increment == 1 ? JsonValue.Create((int)value) : JsonValue.Create((double)value);
                if (owner[property] is null)
                    owner[property] = values;
                commit();
            };
            Grid.SetColumn(box, index % 2 + (rowLabels is null ? 0 : 1));
            Grid.SetRow(box, index / 2);
            row.Children.Add(box);
        }
        field(parent, property, row);
    }

    private static NumericUpDown numeric(double value, double minimum, double maximum, double increment)
    {
        return EditorInputs.CreateNumericUpDown((decimal)Math.Clamp(double.IsFinite(value) ? value : 0, minimum, maximum),
            (decimal)minimum, (decimal)maximum, (decimal)increment);
    }

    private void addBool(StackPanel parent, JsonObject owner, string property, bool fallback)
    {
        CheckBox box = new() { Content = LocaleService.Get("PARTICLE_FIELD_" + property.ToUpperInvariant()), IsChecked = owner[property]?.GetValue<bool>() ?? fallback };
        box.IsCheckedChanged += (_, _) => { owner[property] = box.IsChecked == true; commit(); };
        parent.Children.Add(box);
    }

    private void addChoice(StackPanel parent, JsonObject owner, string property, string[] choices)
    {
        ComboBox box = new() { ItemsSource = choices.Select(choice => LocaleService.Get("PARTICLE_OPTION_" + choice.ToUpperInvariant())).ToArray(),
            SelectedIndex = Math.Max(0, Array.IndexOf(choices, text(owner, property, choices[0]))), HorizontalAlignment = HorizontalAlignment.Stretch };
        box.SelectionChanged += (_, _) =>
        {
            if (box.SelectedIndex < 0)
                return;
            owner[property] = choices[box.SelectedIndex];
            commit();
            if (property == "mode")
                buildProperties();
        };
        field(parent, property, box);
    }

    private void addColour(StackPanel parent, JsonObject owner, string property)
    {
        parent.Children.Add(createExpander("PARTICLE_FIELD_" + property.ToUpperInvariant(), () =>
        {
            JsonArray colour = owner[property] as JsonArray ?? ParticleAssetSchema.Array(255, 255, 255, 255);
            LudorkColourPicker picker = new(Color.FromArgb((byte)number(colour[3], 255),
                (byte)number(colour[0], 255), (byte)number(colour[1], 255), (byte)number(colour[2], 255)));
            picker.ColourChanged += (_, _) =>
            {
                Color value = picker.Color;
                owner[property] = ParticleAssetSchema.Array(value.R, value.G, value.B, value.A);
                commit();
            };
            return picker;
        }));
    }

    private void addTexture(StackPanel parent, JsonObject owner)
    {
        TextBox value = EditorInputs.CreateReadOnlyTextBox(text(owner, "texture"));
        Button browse = button("PARTICLE_BROWSE", () => { });
        browse.Click += async (_, _) =>
        {
            if (TopLevel.GetTopLevel(this) is not Window window)
                return;
            string? selected = await FileSelectorDialog.ShowAsync(window,
                System.IO.Path.Combine(gameData.ProjectPath, "Assets"), FileSelectorDialog.FilesFilter("*.png", "*.jpg", "*.webp"),
                LocaleService.Get("PARTICLE_SELECT_TEXTURE"));
            if (selected is not null && GameAssetPath.TryFromProjectFile(gameData.ProjectPath, selected, out string path))
            {
                owner["texture"] = path;
                value.Text = path;
                commit();
            }
        };
        StackPanel row = new() { Spacing = 4, Children = { value, browse } };
        field(parent, "texture", row);
    }

    private void addBursts(StackPanel parent, JsonObject owner)
    {
        parent.Children.Add(createExpander("PARTICLE_BURSTS", () =>
        {
            StackPanel entries = new() { Spacing = 8 };
            if (owner["bursts"] is JsonArray bursts)
            {
                foreach (JsonObject burst in bursts.OfType<JsonObject>().ToArray())
                {
                    StackPanel fields = new() { Spacing = 4 };
                    addNumber(fields, burst, "time", 0, 0, 3600);
                    addNumber(fields, burst, "count", 16, 0, 1000000, 1);
                    addNumber(fields, burst, "cycles", 1, 1, 10000, 1);
                    addNumber(fields, burst, "interval", 0, 0, 3600);
                    fields.Children.Add(button("PARTICLE_REMOVE_BURST", () => { bursts.Remove(burst); commit(); buildProperties(); }));
                    entries.Children.Add(new Border { Child = fields, Padding = new Thickness(6), BorderThickness = new Thickness(1), BorderBrush = Brushes.DimGray });
                }
            }
            entries.Children.Add(button("PARTICLE_ADD_BURST", () =>
            {
                if (owner["bursts"] is not JsonArray)
                    owner["bursts"] = new JsonArray();
                ((JsonArray)owner["bursts"]!).Add(new JsonObject { ["time"] = 0, ["count"] = 16, ["cycles"] = 1, ["interval"] = 0 });
                commit();
                buildProperties();
            }));
            return entries;
        }));
    }
}
