using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
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
    private readonly string key;
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
    private readonly TextBlock noSelection = new() { Text = LocaleService.Get("NO_SELECTION"), HorizontalAlignment = HorizontalAlignment.Center };
    private readonly string curveType;
    private readonly int componentCount;
    private bool syncing;

    public CurveEditor(GameDataService gameData, string key, JsonObject data)
    {
        this.gameData = gameData;
        this.key = key;
        this.data = (JsonObject)data.DeepClone();
        curveType = normalizeCurveType(this.data["type"]?.GetValue<string>());
        componentCount = curveComponentCount(curveType);
        normalizeData();
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

    public event EventHandler? Modified;

    public void Reload(JsonObject nextData)
    {
        data = (JsonObject)nextData.DeepClone();
        normalizeData();
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
        componentList.Background = new SolidColorBrush(Color.Parse("#282828"));
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
        nameBox.TextChanged += (_, _) => updateGeneral();
        foreach (TextBox box in defaultValueBoxes)
            box.TextChanged += (_, _) => updateGeneral();
        preInfinityBox.SelectionChanged += (_, _) => updateGeneral();
        postInfinityBox.SelectionChanged += (_, _) => updateGeneral();

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
            Foreground = new SolidColorBrush(Color.Parse("#777777")),
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
            box.TextChanged += (_, _) => updateInspector();
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

    private void updateGeneral()
    {
        if (syncing)
            return;
        double[] defaultValue = new double[componentCount];
        for (int index = 0; index < componentCount; index += 1)
        {
            if (!tryNumber(defaultValueBoxes[index].Text, out defaultValue[index]))
                return;
        }
        data["name"] = nameBox.Text ?? string.Empty;
        data["defaultValue"] = valueJson(defaultValue);
        data["preInfinity"] = infinity(preInfinityBox.SelectedItem as string);
        data["postInfinity"] = infinity(postInfinityBox.SelectedItem as string);
        canvas.SetCurveData(
            keys(),
            defaultValue,
            infinity(preInfinityBox.SelectedItem as string),
            infinity(postInfinityBox.SelectedItem as string),
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
        normalizeData();
        gameData.UpdateCurve(key, data);
        Modified?.Invoke(this, EventArgs.Empty);
    }

    private void normalizeData()
    {
        data["name"] ??= key.Split('/').LastOrDefault() ?? string.Empty;
        data["type"] = curveType;
        data["defaultValue"] = valueJson(vector(data["defaultValue"], componentCount));
        data["preInfinity"] = infinity(data["preInfinity"]?.GetValue<string>());
        data["postInfinity"] = infinity(data["postInfinity"]?.GetValue<string>());
        JsonArray normalized = new();
        foreach (JsonNode? node in data["keys"] as JsonArray ?? [])
        {
            if (node is not JsonObject item)
                continue;
            normalized.Add(new CurveKey(item, componentCount).ToJson());
        }
        data["keys"] = normalized;
    }

    private JsonArray keys() => data["keys"] as JsonArray ?? new JsonArray();
    private static bool tryNumber(string? text, out double value) => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value);
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

public sealed class CurveCanvas : Control
{
    private readonly List<CurveKey> keys = [];
    private readonly EditorZoomInput zoomInput = new();
    private double[] defaultValue = [0];
    private string preInfinity = "constant";
    private string postInfinity = "constant";
    private int componentCount = 1;
    private int selectedIndex = -1;
    private int selectedComponent;
    private double minTime = -0.25;
    private double maxTime = 1.25;
    private double minValue = -0.25;
    private double maxValue = 1.25;
    private string dragMode = string.Empty;
    private string dragTangentSide = string.Empty;
    private Point dragStart;
    private const double GraphMargin = 48;

    public CurveCanvas()
    {
        Focusable = true;
        MinWidth = 320;
        MinHeight = 240;
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
    }

    public event Action? DataChanged;
    public event Action<int>? SelectionChanged;
    public CurveKey? SelectedKey => selectedIndex >= 0 && selectedIndex < keys.Count ? keys[selectedIndex] : null;
    public int SelectedComponent => selectedComponent;

    public void SetCurveData(
        JsonArray source,
        IReadOnlyList<double> defaultValue,
        string preInfinity,
        string postInfinity,
        int componentCount)
    {
        this.componentCount = Math.Clamp(componentCount, 1, 4);
        keys.Clear();
        foreach (JsonNode? node in source)
        {
            if (node is JsonObject item)
                keys.Add(new CurveKey(item, this.componentCount));
        }
        keys.Sort((left, right) => left.Time.CompareTo(right.Time));
        this.defaultValue = defaultValue.Take(this.componentCount).ToArray();
        if (this.defaultValue.Length != this.componentCount)
            this.defaultValue = new double[this.componentCount];
        this.preInfinity = CurveEditor.infinity(preInfinity);
        this.postInfinity = CurveEditor.infinity(postInfinity);
        selectedComponent = Math.Clamp(selectedComponent, 0, this.componentCount - 1);
        if (selectedIndex >= keys.Count)
            select(-1);
        InvalidateVisual();
    }

    public JsonArray ExportKeys()
    {
        JsonArray result = new();
        foreach (CurveKey key in keys.OrderBy(key => key.Time))
            result.Add(key.ToJson());
        return result;
    }

    public void FitView()
    {
        if (keys.Count == 0)
        {
            minTime = -0.25;
            maxTime = 1.25;
            double lowDefault = defaultValue.Min();
            double highDefault = defaultValue.Max();
            double defaultSpan = Math.Max(0.05, highDefault - lowDefault);
            minValue = lowDefault - defaultSpan * 0.2;
            maxValue = highDefault + defaultSpan * 0.2;
        }
        else
        {
            double lowTime = keys.Min(key => key.Time);
            double highTime = keys.Max(key => key.Time);
            double lowValue = keys.Min(key => key.Value.Min());
            double highValue = keys.Max(key => key.Value.Max());
            double timeSpan = Math.Max(0.05, highTime - lowTime);
            double valueSpan = Math.Max(0.05, highValue - lowValue);
            minTime = lowTime - timeSpan * 0.15;
            maxTime = highTime + timeSpan * 0.15;
            minValue = lowValue - valueSpan * 0.2;
            maxValue = highValue + valueSpan * 0.2;
        }
        InvalidateVisual();
    }

    public void SelectComponent(int component)
    {
        int normalized = Math.Clamp(component, 0, componentCount - 1);
        if (selectedComponent == normalized)
            return;
        selectedComponent = normalized;
        SelectionChanged?.Invoke(selectedIndex);
        InvalidateVisual();
    }

    public void DeleteSelectedKey()
    {
        if (selectedIndex < 0 || selectedIndex >= keys.Count)
            return;
        keys.RemoveAt(selectedIndex);
        select(-1);
        DataChanged?.Invoke();
        InvalidateVisual();
    }

    public void UpdateSelectedKey(double time, double value, string interpolation, double arriveTangent, double leaveTangent)
    {
        if (SelectedKey is not CurveKey selected)
            return;
        selected.Time = clampTime(selectedIndex, time);
        selected.Value[selectedComponent] = value;
        selected.Interpolation = CurveEditor.interpolation(interpolation);
        selected.ArriveTangent[selectedComponent] = arriveTangent;
        selected.LeaveTangent[selectedComponent] = leaveTangent;
        keys.Sort((left, right) => left.Time.CompareTo(right.Time));
        selectedIndex = keys.IndexOf(selected);
        SelectionChanged?.Invoke(selectedIndex);
        DataChanged?.Invoke();
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#2b2b2b")), bounds);
        Rect graph = graphRect();
        context.FillRectangle(new SolidColorBrush(Color.Parse("#242424")), graph);
        drawGrid(context, graph);
        drawCurve(context, graph);
        drawKeys(context);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        Point position = e.GetPosition(this);
        PointerPointProperties properties = e.GetCurrentPoint(this).Properties;
        if (properties.IsMiddleButtonPressed)
        {
            dragMode = "pan";
            dragStart = position;
            e.Pointer.Capture(this);
            return;
        }
        if (!properties.IsLeftButtonPressed)
            return;
        Focus();
        CurveHit hit = hitKey(position);
        if (e.ClickCount == 2 && graphRect().Contains(position) && hit.Key < 0)
        {
            double time = xToTime(position.X);
            double[] value = new double[componentCount];
            for (int component = 0; component < componentCount; component += 1)
                value[component] = evaluate(time, component);
            CurveKey added = new(new JsonObject
            {
                ["time"] = time,
                ["value"] = CurveEditor.valueJson(value),
                ["interpolation"] = "linear",
                ["arriveTangent"] = CurveEditor.valueJson(new double[componentCount]),
                ["leaveTangent"] = CurveEditor.valueJson(new double[componentCount]),
            }, componentCount);
            keys.Add(added);
            keys.Sort((left, right) => left.Time.CompareTo(right.Time));
            select(keys.IndexOf(added));
            DataChanged?.Invoke();
            InvalidateVisual();
            return;
        }
        if (selectedIndex >= 0 && hitTangent(position, selectedIndex) is string selectedTangent && !string.IsNullOrEmpty(selectedTangent))
        {
            dragMode = "tangent";
            dragTangentSide = selectedTangent;
        }
        else if (hit.Key >= 0)
        {
            select(hit.Key);
            SelectComponent(hit.Component);
            dragMode = "key";
        }
        else
            select(-1);
        dragStart = position;
        e.Pointer.Capture(this);
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        if (string.IsNullOrEmpty(dragMode))
            return;
        Point point = e.GetPosition(this);
        if (dragMode == "pan")
        {
            Rect graph = graphRect();
            Point delta = point - dragStart;
            double timeSpan = maxTime - minTime;
            double valueSpan = maxValue - minValue;
            minTime -= delta.X / graph.Width * timeSpan; maxTime -= delta.X / graph.Width * timeSpan;
            minValue += delta.Y / graph.Height * valueSpan; maxValue += delta.Y / graph.Height * valueSpan;
            dragStart = point;
            InvalidateVisual();
            return;
        }
        if (SelectedKey is not CurveKey selected)
            return;
        if (dragMode == "key")
        {
            selected.Time = clampTime(selectedIndex, xToTime(point.X));
            selected.Value[selectedComponent] = yToValue(point.Y);
        }
        else if (dragMode == "tangent")
        {
            if (dragTangentSide == "leave")
            {
                double dt = xToTime(point.X) - selected.Time;
                if (Math.Abs(dt) > 0.000001)
                {
                    selected.LeaveTangent[selectedComponent] =
                        (yToValue(point.Y) - selected.Value[selectedComponent]) / dt;
                    selected.Interpolation = "cubic";
                }
            }
            else
            {
                double dt = selected.Time - xToTime(point.X);
                if (Math.Abs(dt) > 0.000001)
                {
                    selected.ArriveTangent[selectedComponent] =
                        (selected.Value[selectedComponent] - yToValue(point.Y)) / dt;
                    if (selectedIndex > 0)
                        keys[selectedIndex - 1].Interpolation = "cubic";
                }
            }
        }
        DataChanged?.Invoke();
        InvalidateVisual();
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        dragMode = string.Empty;
        dragTangentSide = string.Empty;
        e.Pointer.Capture(null);
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
    {
        Point point = e.GetPosition(this);
        if (!graphRect().Contains(point))
            return;
        if (zoomInput.ShouldSuppressWheel())
        {
            e.Handled = true;
            return;
        }
        if (EditorZoomInput.IsMacOS)
        {
            if (!EditorZoomInput.HasPrimaryModifier(e.KeyModifiers))
            {
                panByScreenDelta(
                    EditorZoomInput.GetPannedTranslation(default, e.Delta));
                InvalidateVisual();
                e.Handled = true;
                return;
            }
            if (e.Delta.Y == 0)
                return;
            double macFactor = e.Delta.Y > 0 ? 0.9 : 1.1;
            zoomTime(xToTime(point.X), macFactor);
            zoomValue(yToValue(point.Y), macFactor);
            InvalidateVisual();
            e.Handled = true;
            return;
        }
        double factor = e.Delta.Y > 0 ? 0.9 : 1.1;
        if (EditorShortcuts.HasPrimaryModifier(e.KeyModifiers))
            zoomValue(yToValue(point.Y), factor);
        else
            zoomTime(xToTime(point.X), factor);
        InvalidateVisual();
        e.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        Point point = args.GetPosition(this);
        if (!graphRect().Contains(point))
            return;
        zoomInput.MarkMagnify();
        double factor = 1.0 / EditorZoomInput.GetMagnifyFactor(args.Delta.Y);
        zoomTime(xToTime(point.X), factor);
        zoomValue(yToValue(point.Y), factor);
        InvalidateVisual();
        args.Handled = true;
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Key is Key.Delete or Key.Back)
        {
            DeleteSelectedKey();
            e.Handled = true;
            return;
        }
        base.OnKeyDown(e);
    }

    private Rect graphRect()
    {
        return new Rect(GraphMargin, GraphMargin, Math.Max(1, Bounds.Width - GraphMargin * 2), Math.Max(1, Bounds.Height - GraphMargin * 2));
    }

    private double timeToX(double time) => graphRect().Left + (time - minTime) / Math.Max(0.000001, maxTime - minTime) * graphRect().Width;
    private double valueToY(double value) => graphRect().Bottom - (value - minValue) / Math.Max(0.000001, maxValue - minValue) * graphRect().Height;
    private double xToTime(double x) => minTime + (x - graphRect().Left) / graphRect().Width * (maxTime - minTime);
    private double yToValue(double y) => minValue + (graphRect().Bottom - y) / graphRect().Height * (maxValue - minValue);

    private void drawGrid(DrawingContext context, Rect graph)
    {
        Pen grid = new(new SolidColorBrush(Color.Parse("#3a3a3a")), 1);
        for (int index = 0; index <= 10; index += 1)
        {
            double x = graph.Left + graph.Width * index / 10;
            double y = graph.Top + graph.Height * index / 10;
            context.DrawLine(grid, new Point(x, graph.Top), new Point(x, graph.Bottom));
            context.DrawLine(grid, new Point(graph.Left, y), new Point(graph.Right, y));
        }
        context.DrawRectangle(new Pen(new SolidColorBrush(Color.Parse("#666666")), 1), graph);
    }

    private void drawCurve(DrawingContext context, Rect graph)
    {
        if (keys.Count == 0)
            return;
        for (int component = 0; component < componentCount; component += 1)
        {
            if (component != selectedComponent)
                drawCurveComponent(context, graph, component, 2);
        }
        drawCurveComponent(context, graph, selectedComponent, componentCount > 1 ? 3 : 2);
    }

    private void drawCurveComponent(
        DrawingContext context,
        Rect graph,
        int component,
        double thickness)
    {
        int samples = Math.Max(32, (int)graph.Width);
        Pen curvePen = new(new SolidColorBrush(componentColour(component)), thickness);
        Point previous = new(timeToX(minTime), valueToY(evaluate(minTime, component)));
        for (int index = 1; index <= samples; index += 1)
        {
            double time = minTime + (maxTime - minTime) * index / samples;
            Point current = new(timeToX(time), valueToY(evaluate(time, component)));
            context.DrawLine(curvePen, previous, current);
            previous = current;
        }
    }

    private void drawKeys(DrawingContext context)
    {
        if (SelectedKey is not null)
        {
            if (selectedIndex > 0)
                drawTangent(context, selectedIndex, selectedComponent, "arrive");
            if (selectedIndex < keys.Count - 1)
                drawTangent(context, selectedIndex, selectedComponent, "leave");
        }
        for (int component = 0; component < componentCount; component += 1)
        {
            if (component != selectedComponent)
                drawKeyComponent(context, component);
        }
        drawKeyComponent(context, selectedComponent);
    }

    private void drawKeyComponent(DrawingContext context, int component)
    {
        for (int index = 0; index < keys.Count; index += 1)
        {
            CurveKey key = keys[index];
            Point point = new(timeToX(key.Time), valueToY(key.Value[component]));
            bool isSelected = index == selectedIndex && component == selectedComponent;
            context.DrawEllipse(
                new SolidColorBrush(componentColour(component)),
                new Pen(
                    isSelected
                        ? Brushes.White
                        : new SolidColorBrush(Color.Parse("#dddddd")),
                    1),
                point,
                isSelected ? 5 : 4,
                isSelected ? 5 : 4);
        }
    }

    private void drawTangent(
        DrawingContext context,
        int index,
        int component,
        string side)
    {
        CurveKey key = keys[index];
        (double time, double value) = tangentHandle(index, component, side);
        Point keyPoint = new(timeToX(key.Time), valueToY(key.Value[component]));
        Point handlePoint = new(timeToX(time), valueToY(value));
        context.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#6ab4ff")), 1), keyPoint, handlePoint);
        context.DrawEllipse(new SolidColorBrush(Color.Parse("#6ab4ff")), null, handlePoint, 4, 4);
    }

    private CurveHit hitKey(Point point)
    {
        CurveHit selectedHit = hitKeyComponent(point, selectedComponent);
        if (selectedHit.Key >= 0)
            return selectedHit;
        for (int component = componentCount - 1; component >= 0; component -= 1)
        {
            if (component == selectedComponent)
                continue;
            CurveHit hit = hitKeyComponent(point, component);
            if (hit.Key >= 0)
                return hit;
        }
        return new CurveHit(-1, -1);
    }

    private CurveHit hitKeyComponent(Point point, int component)
    {
        for (int index = keys.Count - 1; index >= 0; index -= 1)
        {
            if (distance(
                    point.X - timeToX(keys[index].Time),
                    point.Y - valueToY(keys[index].Value[component])) <= 7)
            {
                return new CurveHit(index, component);
            }
        }
        return new CurveHit(-1, -1);
    }

    private string hitTangent(Point point, int index)
    {
        if (index < 0 || index >= keys.Count)
            return string.Empty;
        foreach (string side in new[] { "leave", "arrive" })
        {
            if (side == "leave" && index >= keys.Count - 1 || side == "arrive" && index <= 0)
                continue;
            (double time, double value) = tangentHandle(index, selectedComponent, side);
            if (distance(point.X - timeToX(time), point.Y - valueToY(value)) <= 10)
                return side;
        }
        return string.Empty;
    }

    private (double time, double value) tangentHandle(
        int index,
        int component,
        string side)
    {
        CurveKey key = keys[index];
        double span = side == "leave" && index < keys.Count - 1
            ? Math.Max((keys[index + 1].Time - key.Time) / 3, 0.33)
            : side == "arrive" && index > 0 ? Math.Max((key.Time - keys[index - 1].Time) / 3, 0.33) : 0.33;
        double tangent = side == "leave"
            ? key.LeaveTangent[component]
            : key.ArriveTangent[component];
        if (Math.Abs(tangent) < 0.0000001)
        {
            if (side == "leave" && index < keys.Count - 1)
                tangent = slope(key, keys[index + 1], component);
            else if (side == "arrive" && index > 0)
                tangent = slope(keys[index - 1], key, component);
        }
        return side == "leave"
            ? (key.Time + span, key.Value[component] + tangent * span)
            : (key.Time - span, key.Value[component] - tangent * span);
    }

    private void select(int index)
    {
        if (index < -1 || index >= keys.Count)
            index = -1;
        if (selectedIndex == index)
            return;
        selectedIndex = index;
        SelectionChanged?.Invoke(index);
    }

    private double clampTime(int index, double time)
    {
        double low = index > 0 ? keys[index - 1].Time + 0.0001 : minTime;
        double high = index < keys.Count - 1 ? keys[index + 1].Time - 0.0001 : maxTime;
        return Math.Clamp(time, low, high);
    }

    private void zoomTime(double anchor, double factor)
    {
        double span = Math.Max(0.05, (maxTime - minTime) * factor);
        double ratio = (anchor - minTime) / Math.Max(0.000001, maxTime - minTime);
        minTime = anchor - span * ratio;
        maxTime = minTime + span;
    }

    private void zoomValue(double anchor, double factor)
    {
        double span = Math.Max(0.05, (maxValue - minValue) * factor);
        double ratio = (anchor - minValue) / Math.Max(0.000001, maxValue - minValue);
        minValue = anchor - span * ratio;
        maxValue = minValue + span;
    }

    private void panByScreenDelta(Vector delta)
    {
        Rect graph = graphRect();
        double timeDelta = delta.X / graph.Width * (maxTime - minTime);
        double valueDelta = delta.Y / graph.Height * (maxValue - minValue);
        minTime -= timeDelta;
        maxTime -= timeDelta;
        minValue += valueDelta;
        maxValue += valueDelta;
    }

    private double evaluate(double time, int component)
    {
        if (keys.Count == 0)
            return defaultValue[component];
        if (keys.Count == 1)
            return keys[0].Value[component];
        if (time <= keys[0].Time)
            return extrapolate(time, keys[0], keys[1], component, preInfinity, true);
        if (time >= keys[^1].Time)
            return extrapolate(time, keys[^2], keys[^1], component, postInfinity, false);
        for (int index = 0; index < keys.Count - 1; index += 1)
        {
            if (time >= keys[index].Time && time <= keys[index + 1].Time)
                return evaluateSegment(keys[index], keys[index + 1], component, time);
        }
        return keys[^1].Value[component];
    }

    private static double evaluateSegment(
        CurveKey start,
        CurveKey end,
        int component,
        double time)
    {
        double duration = end.Time - start.Time;
        if (duration <= 0)
            return end.Value[component];
        if (start.Interpolation == "constant")
            return start.Value[component];
        double alpha = (time - start.Time) / duration;
        if (start.Interpolation != "cubic")
        {
            return start.Value[component]
                + (end.Value[component] - start.Value[component]) * alpha;
        }
        double t2 = alpha * alpha;
        double t3 = t2 * alpha;
        return (2 * t3 - 3 * t2 + 1) * start.Value[component]
            + (t3 - 2 * t2 + alpha) * start.LeaveTangent[component] * duration
            + (-2 * t3 + 3 * t2) * end.Value[component]
            + (t3 - t2) * end.ArriveTangent[component] * duration;
    }

    private static double extrapolate(
        double time,
        CurveKey start,
        CurveKey end,
        int component,
        string mode,
        bool beforeFirst)
    {
        CurveKey edge = beforeFirst ? start : end;
        return mode != "linear"
            ? edge.Value[component]
            : edge.Value[component]
                + (time - edge.Time) * slope(start, end, component);
    }

    private static double slope(CurveKey start, CurveKey end, int component)
    {
        return Math.Abs(end.Time - start.Time) < 0.000001
            ? 0
            : (end.Value[component] - start.Value[component])
                / (end.Time - start.Time);
    }
    internal static Color componentColour(int component)
    {
        return component switch
        {
            1 => Color.Parse("#65d96f"),
            2 => Color.Parse("#5a9cff"),
            3 => Color.Parse("#e6d65c"),
            _ => Color.Parse("#ff5a5a"),
        };
    }
    private static double distance(double x, double y) => Math.Sqrt(x * x + y * y);

    private readonly record struct CurveHit(int Key, int Component);
}

public sealed class CurveKey
{
    public CurveKey(JsonObject data, int componentCount)
    {
        Time = CurveEditor.number(data["time"]);
        Value = CurveEditor.vector(data["value"], componentCount);
        Interpolation = CurveEditor.interpolation(data["interpolation"]?.GetValue<string>());
        ArriveTangent = CurveEditor.vector(data["arriveTangent"], componentCount);
        LeaveTangent = CurveEditor.vector(data["leaveTangent"], componentCount);
    }

    public double Time { get; set; }
    public double[] Value { get; }
    public string Interpolation { get; set; }
    public double[] ArriveTangent { get; }
    public double[] LeaveTangent { get; }

    public JsonObject ToJson() => new()
    {
        ["time"] = Time,
        ["value"] = CurveEditor.valueJson(Value),
        ["interpolation"] = Interpolation,
        ["arriveTangent"] = CurveEditor.valueJson(ArriveTangent),
        ["leaveTangent"] = CurveEditor.valueJson(LeaveTangent),
    };
}
