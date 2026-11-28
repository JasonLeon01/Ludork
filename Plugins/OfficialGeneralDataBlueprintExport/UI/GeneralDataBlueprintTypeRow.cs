using Avalonia;
using Avalonia.Automation;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using System;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport.UI;

internal sealed class GeneralDataBlueprintTypeRow : Border
{
    internal const string Columns = "32,2*,5*";
    private readonly CheckBox check = new();
    private readonly TextBox directory;

    public string TypeKey { get; }
    public bool IsSelected => check.IsChecked == true;
    public string Directory { get => directory.Text ?? string.Empty; set => directory.Text = value; }
    public event EventHandler? SelectionChanged;
    public event EventHandler? BrowseRequested;

    public GeneralDataBlueprintTypeRow(string typeKey, PluginLocalizer localizer)
    {
        TypeKey = typeKey;
        Padding = new Thickness(8, 4);
        CornerRadius = new CornerRadius(4);
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions(Columns), ColumnSpacing = 8 };
        AutomationProperties.SetName(check, typeKey);
        row.Children.Add(check);
        TextBlock name = new()
        {
            Text = typeKey,
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
            Foreground = PluginTheme.Brush("TextDisabled"),
        };
        ToolTip.SetTip(name, typeKey);
        Grid.SetColumn(name, 1);
        row.Children.Add(name);

        Grid destination = new() { ColumnDefinitions = new ColumnDefinitions("*,Auto"), ColumnSpacing = 6, IsVisible = false };
        directory = EditorInputs.CreateEditableTextBox($"Data/Blueprints/{typeKey}s");
        AutomationProperties.SetName(directory, $"{typeKey} · {localizer.Text("directory")}");
        destination.Children.Add(directory);
        Button browse = new() { Content = localizer.Text("browse") };
        browse.Click += (_, _) => BrowseRequested?.Invoke(this, EventArgs.Empty);
        Grid.SetColumn(browse, 1);
        destination.Children.Add(browse);
        Grid.SetColumn(destination, 2);
        row.Children.Add(destination);
        Child = row;

        check.IsCheckedChanged += (_, _) =>
        {
            bool selected = IsSelected;
            Background = selected ? PluginTheme.Brush("AccentMuted") : null;
            name.Foreground = PluginTheme.Brush(selected ? "Text" : "TextDisabled");
            destination.IsVisible = selected;
            SelectionChanged?.Invoke(this, EventArgs.Empty);
        };
    }
}
