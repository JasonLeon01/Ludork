using Ludork.Models;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class BlueprintStructureWindow : Window
{
    private readonly BlueprintVariableForm variableForm;
    private readonly JsonObject value;
    private bool hasEdits;

    private BlueprintStructureWindow(
        string title,
        IReadOnlyList<BlueprintVariableField> fields,
        JsonObject? initialValue,
        string assetsDirectory,
        int cellSize,
        IGameVariableCatalog? gameVariables,
        bool readOnly,
        Func<JsonObject, IReadOnlyList<BlueprintVariableField>>? createFields)
    {
        Title = title;
        double contentHeight = Math.Max(200, fields.Count * 38 + 68);
        Width = 520;
        Height = Math.Min(contentHeight, 640);
        MinWidth = 420;
        MinHeight = Math.Min(contentHeight, 640);
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        value = initialValue?.DeepClone() as JsonObject ?? [];
        variableForm = new BlueprintVariableForm
        {
            AssetsDirectory = assetsDirectory,
            CellSize = cellSize,
            GameVariables = gameVariables,
            IsReadOnly = readOnly,
        };
        variableForm.ValueChanged += (_, args) =>
        {
            value[args.Name] = args.Value?.DeepClone();
            hasEdits = true;
            if (args.RequiresRefresh && createFields is not null)
                variableForm.SetFields(createFields(value));
        };
        variableForm.SetFields(createFields?.Invoke(value) ?? fields);
        Closed += (_, _) => variableForm.Dispose();

        ScrollViewer scroll = new()
        {
            Content = variableForm,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        Button confirm = new()
        {
            Content = LocaleService.Get("CONFIRM"),
            IsEnabled = !readOnly,
        };
        confirm.Click += (_, _) => Close(hasEdits && !JsonNode.DeepEquals(initialValue, value)
            ? value.DeepClone() as JsonObject
            : null);
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close(null);
        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };
        Grid layout = new()
        {
            Margin = new Thickness(10),
            RowDefinitions = new RowDefinitions("*,Auto"),
            RowSpacing = 8,
        };
        layout.Children.Add(scroll);
        Grid.SetRow(actions, 1);
        layout.Children.Add(actions);
        Content = layout;
        KeyDown += (_, args) =>
        {
            if (args.Key != Key.Escape)
                return;
            Close(null);
            args.Handled = true;
        };
    }

    public static System.Threading.Tasks.Task<JsonObject?> ShowAsync(
        Window owner,
        string title,
        IReadOnlyList<BlueprintVariableField> fields,
        JsonObject? initialValue,
        string assetsDirectory,
        int cellSize,
        IGameVariableCatalog? gameVariables,
        bool readOnly,
        Func<JsonObject, IReadOnlyList<BlueprintVariableField>>? createFields = null)
    {
        BlueprintStructureWindow window = new(
            title,
            fields,
            initialValue,
            assetsDirectory,
            cellSize,
            gameVariables,
            readOnly,
            createFields);
        return window.ShowDialog<JsonObject?>(owner);
    }
}
