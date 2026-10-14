using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

internal sealed class MaterialParameterCreationWindow : Window
{
    public MaterialParameterCreationWindow(JsonObject registry, Func<string, bool> isAvailable)
    {
        Title = LocaleService.Get("MATERIAL_ADD_PARAMETER");
        Width = 360;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);
        StackPanel panel = new() { Spacing = 10, Margin = new Thickness(16) };
        TextBox id = EditorInputs.CreateEditableTextBox();
        panel.Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_PARAMETER_ID") });
        panel.Children.Add(id);
        ComboBox type = new()
        {
            ItemsSource = ((JsonArray)registry["parameterTypes"]!).Select(value => value!.GetValue<string>()).ToArray(),
            SelectedIndex = 0, HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        panel.Children.Add(new TextBlock { Text = LocaleService.Get("ATTRIBUTE_DECLARED_TYPE") });
        panel.Children.Add(type);
        TextBlock error = new()
        {
            Text = LocaleService.Get("MATERIAL_PARAMETER_ID_INVALID"),
            Foreground = EditorTheme.Brush("Error"), TextWrapping = TextWrapping.Wrap, IsVisible = false,
        };
        panel.Children.Add(error);
        Button confirm = new() { Content = LocaleService.Get("OK"), IsDefault = true };
        confirm.Click += (_, _) =>
        {
            string name = id.Text?.Trim() ?? string.Empty;
            if (name.Length == 0 || !isAvailable(name))
            {
                error.IsVisible = true;
                id.Focus();
                return;
            }
            string parameterType = (string)type.SelectedItem!;
            Close(new JsonObject
            {
                ["id"] = name, ["displayName"] = name, ["type"] = parameterType,
                ["default"] = registry["parameterDefaults"]![parameterType]!.DeepClone(), ["group"] = string.Empty,
            });
        };
        Button cancel = new() { Content = LocaleService.Get("CANCEL"), IsCancel = true };
        cancel.Click += (_, _) => Close();
        StackPanel actions = new() { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Spacing = 8 };
        actions.Children.Add(confirm);
        actions.Children.Add(cancel);
        panel.Children.Add(actions);
        Content = panel;
        Opened += (_, _) => id.Focus();
    }
}
