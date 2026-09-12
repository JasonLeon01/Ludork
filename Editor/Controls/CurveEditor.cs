using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class CurveEditor : UserControl
{
    private readonly GameDataService gameData;
    private readonly EditorDocument? resourceDocument;
    private readonly string initialKey;
    private string key => resourceDocument?.Key ?? initialKey;
    private JsonObject data;
    private readonly CurveCanvas canvas = new();
    private readonly TextBox nameBox = EditorInputs.CreateEditableTextBox();
    private readonly List<TextBox> defaultValueBoxes = [];
    private readonly ComboBox preInfinityBox = new();
    private readonly ComboBox postInfinityBox = new();
    private readonly ComboBox componentBox = new();
    private readonly ListBox componentList = new();
    private readonly TextBox timeBox = EditorInputs.CreateEditableTextBox();
    private readonly TextBox valueBox = EditorInputs.CreateEditableTextBox();
    private readonly ComboBox interpolationBox = new();
    private readonly TextBox arriveTangentBox = EditorInputs.CreateEditableTextBox();
    private readonly TextBox leaveTangentBox = EditorInputs.CreateEditableTextBox();
    private readonly StackPanel keyInspector = new() { Spacing = 6 };
    private readonly TextBlock noSelection = new() { Text = LocaleService.Get("GENERAL_DATA_PLACEHOLDER"), HorizontalAlignment = HorizontalAlignment.Center };
    private readonly string curveType;
    private readonly int componentCount;
    private bool syncing;

    public CurveEditor(GameDataService gameData, string key, JsonObject data)
    {
        this.gameData = gameData;
        initialKey = key;
        resourceDocument = gameData.GetDocument("Curves", key);
        this.data = (JsonObject)data.DeepClone();
        curveType = normalizeCurveType(this.data["type"]?.GetValue<string>());
        componentCount = curveComponentCount(curveType);
        for (int index = 0; index < componentCount; index += 1)
        {
            TextBox box = EditorInputs.CreateEditableTextBox();
            if (componentCount > 1)
                box.PlaceholderText = componentName(index);
            defaultValueBoxes.Add(box);
        }
        canvas.SelectionChanged += _ => refreshInspector();
        canvas.DataChanged += onCanvasChanged;
        buildLayout();
        refreshEditor(true);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        if (resourceDocument is not null)
            resourceDocument.Changed += onDocumentChanged;
        onDocumentChanged(this, EventArgs.Empty);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        if (resourceDocument is not null)
            resourceDocument.Changed -= onDocumentChanged;
        base.OnDetachedFromVisualTree(args);
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (resourceDocument?.Data is not JsonObject current || JsonNode.DeepEquals(current, data))
            return;
        data = current;
        refreshEditor(false);
    }

    public event EventHandler? Modified;

    public void Reload(JsonObject nextData)
    {
        data = (JsonObject)nextData.DeepClone();
        refreshEditor(true);
    }

    private void buildLayout()
    {
        Grid root = new() { RowDefinitions = new RowDefinitions("Auto,*,Auto"), RowSpacing = 8, Margin = new Thickness(10) };
        Grid form = new() { ColumnDefinitions = new ColumnDefinitions("Auto,180,Auto,*"), RowDefinitions = new RowDefinitions("Auto,Auto"), ColumnSpacing = 8, RowSpacing = 6 };
        addFormField(form, 0, 0, LocaleService.Get("CURVE_NAME"), nameBox);
        addFormField(form, 0, 2, LocaleService.Get("CURVE_DEFAULT_VALUE"), createDefaultValueEditor());
        addFormField(form, 1, 0, LocaleService.Get("CURVE_PRE_INFINITY"), preInfinityBox);
        addFormField(form, 1, 2, LocaleService.Get("CURVE_POST_INFINITY"), postInfinityBox);
        root.Children.Add(form);

        preInfinityBox.ItemsSource = new[] { "constant", "linear" };
        postInfinityBox.ItemsSource = new[] { "constant", "linear" };
        interpolationBox.ItemsSource = new[] { "constant", "linear", "cubic" };
        string[] componentNames = Enumerable.Range(0, componentCount)
            .Select(componentName)
            .ToArray();
        componentBox.ItemsSource = componentNames;
        componentList.ItemsSource = Enumerable.Range(0, componentCount)
            .Select(createComponentListItem)
            .ToArray();
        componentList.Background = Ludork.Services.EditorTheme.Brush("Surface");
        componentList.SelectionMode = SelectionMode.Single;
        componentList.SelectionChanged += (_, _) =>
        {
            if (syncing)
                return;
            if (componentList.SelectedIndex < 0)
            {
                syncing = true;
                componentList.SelectedIndex = canvas.SelectedComponent;
                syncing = false;
                return;
            }
            canvas.SelectComponent(componentList.SelectedIndex);
            refreshInspector();
        };
        nameBox.PropertyChanged += (_, args) =>
        {
            if (args.Property == TextBox.TextProperty)
                updateGeneral("name");
        };
        foreach (TextBox box in defaultValueBoxes)
        {
            box.PropertyChanged += (_, args) =>
            {
                if (args.Property == TextBox.TextProperty)
                    updateGeneral("defaultValue");
            };
        }
        preInfinityBox.SelectionChanged += (_, _) => updateGeneral("preInfinity");
        postInfinityBox.SelectionChanged += (_, _) => updateGeneral("postInfinity");

        Grid center = new()
        {
            ColumnDefinitions = componentCount > 1
                ? new ColumnDefinitions("112,*,250")
                : new ColumnDefinitions("*,250"),
            ColumnSpacing = 8,
        };
        int canvasColumn = 0;
        int inspectorColumn = 1;
        if (componentCount > 1)
        {
            Grid componentPanel = new()
            {
                RowDefinitions = new RowDefinitions("Auto,*"),
                RowSpacing = 6,
            };
            componentPanel.Children.Add(new TextBlock
            {
                Text = LocaleService.Get("CURVE_COMPONENT"),
                FontWeight = FontWeight.Bold,
            });
            Grid.SetRow(componentList, 1);
            componentPanel.Children.Add(componentList);
            center.Children.Add(componentPanel);
            canvasColumn = 1;
            inspectorColumn = 2;
        }
        Grid canvasHost = new();
        string canvasHint = LocaleService.Get(
            EditorZoomInput.IsMacOS
                ? "CURVE_CANVAS_HINT_MACOS"
                : "CURVE_CANVAS_HINT");
        canvasHost.Children.Add(canvas);
        canvasHost.Children.Add(new TextBlock
        {
            Text = canvasHint,
            Foreground = EditorTheme.Brush("TextMuted"),
            FontSize = 11,
            HorizontalAlignment = HorizontalAlignment.Right,
            VerticalAlignment = VerticalAlignment.Top,
            Margin = new Thickness(52, 8, 52, 0),
        });
        Grid.SetColumn(canvasHost, canvasColumn);
        center.Children.Add(canvasHost);
        ScrollViewer inspectorScroll = new() { Content = keyInspector, VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto };
        Grid.SetColumn(inspectorScroll, inspectorColumn);
        center.Children.Add(inspectorScroll);
        Grid.SetRow(center, 1);
        root.Children.Add(center);

        keyInspector.Children.Add(new TextBlock { Text = LocaleService.Get("CURVE_KEY_PROPERTIES"), FontWeight = FontWeight.Bold });
        Grid componentRow = addInspectorField(LocaleService.Get("CURVE_COMPONENT"), componentBox);
        componentRow.IsVisible = componentCount > 1;
        addInspectorField(LocaleService.Get("time"), timeBox);
        addInspectorField(LocaleService.Get("CURVE_VALUE"), valueBox);
        addInspectorField(LocaleService.Get("CURVE_INTERPOLATION"), interpolationBox);
        addInspectorField(LocaleService.Get("CURVE_ARRIVE_TANGENT"), arriveTangentBox);
        addInspectorField(LocaleService.Get("CURVE_LEAVE_TANGENT"), leaveTangentBox);
        keyInspector.Children.Add(noSelection);
        foreach (TextBox box in new[] { timeBox, valueBox, arriveTangentBox, leaveTangentBox })
        {
            box.PropertyChanged += (_, args) =>
            {
                if (args.Property == TextBox.TextProperty)
                    updateInspector();
            };
        }
        interpolationBox.SelectionChanged += (_, _) => updateInspector();
        componentBox.SelectionChanged += (_, _) =>
        {
            if (syncing)
                return;
            canvas.SelectComponent(Math.Max(0, componentBox.SelectedIndex));
            refreshInspector();
        };

        StackPanel buttons = new() { Orientation = Orientation.Horizontal, Spacing = 8 };
        Button fit = new() { Content = LocaleService.Get("CURVE_FIT_VIEW") };
        fit.Click += (_, _) => canvas.FitView();
        Button delete = new() { Content = LocaleService.Get("DELETE") };
        delete.Click += (_, _) => canvas.DeleteSelectedKey();
        buttons.Children.Add(fit);
        buttons.Children.Add(delete);
        Grid.SetRow(buttons, 2);
        root.Children.Add(buttons);
        Content = root;
    }

    private static Control createComponentListItem(int component)
    {
        StackPanel row = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Margin = new Thickness(2, 4),
        };
        row.Children.Add(new Border
        {
            Width = 10,
            Height = 10,
            CornerRadius = new CornerRadius(5),
            Background = new SolidColorBrush(CurveCanvas.componentColour(component)),
            VerticalAlignment = VerticalAlignment.Center,
        });
        row.Children.Add(new TextBlock
        {
            Text = componentName(component),
            VerticalAlignment = VerticalAlignment.Center,
        });
        return row;
    }

    private Control createDefaultValueEditor()
    {
        Grid grid = new()
        {
            ColumnSpacing = 4,
        };
        for (int index = 0; index < defaultValueBoxes.Count; index += 1)
        {
            grid.ColumnDefinitions.Add(new ColumnDefinition(GridLength.Star));
            Grid.SetColumn(defaultValueBoxes[index], index);
            grid.Children.Add(defaultValueBoxes[index]);
        }
        return grid;
    }

    private static void addFormField(Grid form, int row, int column, string label, Control field)
    {
        TextBlock text = new() { Text = label, VerticalAlignment = VerticalAlignment.Center };
        Grid.SetRow(text, row);
        Grid.SetColumn(text, column);
        form.Children.Add(text);
        Grid.SetRow(field, row);
        Grid.SetColumn(field, column + 1);
        form.Children.Add(field);
    }

    private Grid addInspectorField(string label, Control field)
    {
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("96,*"), ColumnSpacing = 6 };
        row.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center });
        Grid.SetColumn(field, 1);
        row.Children.Add(field);
        keyInspector.Children.Add(row);
        return row;
    }

    private void refreshEditor(bool fitView)
    {
        syncing = true;
        nameBox.Text = data["name"]?.GetValue<string>() ?? key;
        double[] defaultValues = vector(data["defaultValue"], componentCount);
        for (int index = 0; index < defaultValueBoxes.Count; index += 1)
            defaultValueBoxes[index].Text = defaultValues[index].ToString(CultureInfo.InvariantCulture);
        preInfinityBox.SelectedItem = infinity(data["preInfinity"]?.GetValue<string>());
        postInfinityBox.SelectedItem = infinity(data["postInfinity"]?.GetValue<string>());
        canvas.SetCurveData(
            keys(),
            defaultValues,
            infinity(data["preInfinity"]?.GetValue<string>()),
            infinity(data["postInfinity"]?.GetValue<string>()),
            componentCount);
        syncing = false;
        if (fitView)
            canvas.FitView();
        refreshInspector();
    }

    private void refreshInspector()
    {
        syncing = true;
        CurveKey? selected = canvas.SelectedKey;
        bool hasKey = selected is not null;
        componentBox.SelectedIndex = canvas.SelectedComponent;
        componentList.SelectedIndex = canvas.SelectedComponent;
        foreach (Control control in new Control[] { timeBox, valueBox, interpolationBox, arriveTangentBox, leaveTangentBox })
            control.IsVisible = hasKey;
        noSelection.IsVisible = !hasKey;
        if (selected is not null)
        {
            timeBox.Text = selected.Time.ToString(CultureInfo.InvariantCulture);
            valueBox.Text = selected.Value[canvas.SelectedComponent].ToString(CultureInfo.InvariantCulture);
            interpolationBox.SelectedItem = selected.Interpolation;
            arriveTangentBox.Text = selected.ArriveTangent[canvas.SelectedComponent].ToString(CultureInfo.InvariantCulture);
            leaveTangentBox.Text = selected.LeaveTangent[canvas.SelectedComponent].ToString(CultureInfo.InvariantCulture);
            bool cubic = selected.Interpolation == "cubic";
            arriveTangentBox.IsEnabled = cubic;
            leaveTangentBox.IsEnabled = cubic;
        }
        syncing = false;
    }

    private void updateGeneral(string property)
    {
        if (syncing)
            return;
        JsonNode next;
        JsonNode current;
        if (property == "name")
        {
            next = JsonValue.Create(nameBox.Text ?? string.Empty);
            current = JsonValue.Create(data["name"]?.GetValue<string>() ?? key);
        }
        else if (property == "defaultValue")
        {
            double[] values = new double[componentCount];
            for (int index = 0; index < componentCount; index += 1)
            {
                if (!tryNumber(defaultValueBoxes[index].Text, out values[index]))
                    return;
            }
            next = valueJson(values);
            current = valueJson(vector(data[property], componentCount));
        }
        else
        {
            ComboBox box = property == "preInfinity" ? preInfinityBox : postInfinityBox;
            next = JsonValue.Create(infinity(box.SelectedItem as string));
            current = JsonValue.Create(infinity(data[property]?.GetValue<string>()));
        }
        if (JsonNode.DeepEquals(current, next))
            return;
        data[property] = next;
        canvas.SetCurveData(
            keys(),
            vector(data["defaultValue"], componentCount),
            infinity(data["preInfinity"]?.GetValue<string>()),
            infinity(data["postInfinity"]?.GetValue<string>()),
            componentCount);
        commit();
    }

    private void updateInspector()
    {
        if (syncing || canvas.SelectedKey is null || !tryNumber(timeBox.Text, out double time) || !tryNumber(valueBox.Text, out double value)
            || !tryNumber(arriveTangentBox.Text, out double arrive) || !tryNumber(leaveTangentBox.Text, out double leave))
            return;
        canvas.UpdateSelectedKey(time, value, interpolationBox.SelectedItem as string ?? "linear", arrive, leave);
    }

    private void onCanvasChanged()
    {
        if (syncing)
            return;
        data["keys"] = canvas.ExportKeys();
        refreshInspector();
        commit();
    }

    private void commit()
    {
        gameData.UpdateCurve(key, data);
        Modified?.Invoke(this, EventArgs.Empty);
    }

    private JsonArray keys() => data["keys"] as JsonArray ?? new JsonArray();
    private static bool tryNumber(string? text, out double value) => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value) && double.IsFinite(value);
    internal static double number(JsonNode? node) => node is JsonValue value && value.TryGetValue<double>(out double number) ? number : 0;
    internal static double[] vector(JsonNode? node, int componentCount)
    {
        JsonArray? values = node as JsonArray;
        double[] result = new double[componentCount];
        for (int index = 0; index < componentCount; index += 1)
        {
            result[index] = values is null
                ? index == 0 ? number(node) : 0
                : index < values.Count ? number(values[index]) : 0;
        }
        return result;
    }
    internal static JsonNode valueJson(IReadOnlyList<double> value)
    {
        if (value.Count == 1)
            return JsonValue.Create(value[0]);
        JsonArray result = new();
        foreach (double component in value)
            result.Add(component);
        return result;
    }
    internal static string componentName(int component)
    {
        return component switch
        {
            1 => "Y",
            2 => "Z",
            3 => "W",
            _ => "X",
        };
    }
    private static string normalizeCurveType(string? type)
    {
        return type is "vector2Curve" or "vector3Curve" or "vector4Curve"
            ? type
            : "curve";
    }
    private static int curveComponentCount(string type)
    {
        return type switch
        {
            "vector2Curve" => 2,
            "vector3Curve" => 3,
            "vector4Curve" => 4,
            _ => 1,
        };
    }
    internal static string interpolation(string? value) => value is "constant" or "linear" or "cubic" ? value : "linear";
    internal static string infinity(string? value) => value is "linear" ? "linear" : "constant";
}
