using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Views.Utils;
using Ludork.Services;
using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed class MaterialEditorWindow : Window
{
    private readonly JsonObject material;
    private readonly Action<JsonObject> onAccepted;
    private readonly TextBox lightBlock;
    private readonly CheckBox mirror;
    private readonly TextBox reflectionStrength;
    private readonly TextBox opacity;
    private readonly TextBox speedRate;

    public MaterialEditorWindow(JsonObject material, Action<JsonObject> onAccepted)
    {
        this.material = (JsonObject)material.DeepClone();
        this.onAccepted = onAccepted;
        Title = LocaleService.Get("EDIT_MATERIAL");
        Width = 390;
        Height = 300;
        MinWidth = 360;
        MinHeight = 280;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        Grid form = new() { ColumnDefinitions = new ColumnDefinitions("Auto,12,*"), RowSpacing = 8 };
        lightBlock = addNumberRow(form, 0, "lightBlock", 0.0);
        mirror = addCheckRow(form, 1, "mirror", false);
        reflectionStrength = addNumberRow(form, 2, "reflectionStrength", 0.5);
        opacity = addNumberRow(form, 3, "opacity", 1.0);
        speedRate = addNumberRow(form, 4, "speedRate", 1.0);

        Button confirm = new() { Content = LocaleService.Get("CONFIRM"), MinWidth = 80 };
        confirm.Click += onConfirm;
        Button cancel = new() { Content = LocaleService.Get("CANCEL"), MinWidth = 80 };
        cancel.Click += (_, _) => Close();
        StackPanel buttons = new()
        {
            Orientation = Avalonia.Layout.Orientation.Horizontal,
            Spacing = 8,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Right,
            Children = { cancel, confirm },
        };
        Content = new StackPanel
        {
            Margin = new Thickness(16),
            Spacing = 16,
            Children = { form, buttons },
        };
    }

    private TextBox addNumberRow(Grid form, int row, string key, double fallback)
    {
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        TextBlock label = addLabel(form, row, key);
        TextBox value = EditorInputs.CreateEditableTextBox(getDouble(key, fallback).ToString(CultureInfo.InvariantCulture));
        value.HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch;
        Grid.SetColumn(value, 2);
        Grid.SetRow(value, row);
        form.Children.Add(value);
        return value;
    }

    private CheckBox addCheckRow(Grid form, int row, string key, bool fallback)
    {
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        addLabel(form, row, key);
        CheckBox value = new() { IsChecked = material[key]?.GetValue<bool?>() ?? fallback };
        Grid.SetColumn(value, 2);
        Grid.SetRow(value, row);
        form.Children.Add(value);
        return value;
    }

    private static TextBlock addLabel(Grid form, int row, string key)
    {
        TextBlock label = new() { Text = key, VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center };
        Grid.SetRow(label, row);
        form.Children.Add(label);
        return label;
    }

    private double getDouble(string key, double fallback) => material[key]?.GetValue<double?>() ?? fallback;

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (!tryParse(lightBlock, out double nextLightBlock)
            || !(nextLightBlock >= 0.0 && nextLightBlock <= 1.0)
            || !tryParse(reflectionStrength, out double nextReflectionStrength)
            || !tryParse(opacity, out double nextOpacity)
            || !tryParse(speedRate, out double nextSpeedRate))
            return;
        material["lightBlock"] = nextLightBlock;
        material["mirror"] = mirror.IsChecked == true;
        material["reflectionStrength"] = nextReflectionStrength;
        material["opacity"] = nextOpacity;
        material["speedRate"] = nextSpeedRate;
        onAccepted(material);
        Close();
    }

    private static bool tryParse(TextBox box, out double value)
    {
        return double.TryParse(box.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out value);
    }
}
