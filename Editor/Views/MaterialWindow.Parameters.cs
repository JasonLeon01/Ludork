using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.VisualTree;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed partial class MaterialWindow
{
    private Control buildParameterPane()
    {
        Grid pane = new() { RowDefinitions = new RowDefinitions("Auto,2*,5,3*,Auto") };
        pane.RowDefinitions[1].MinHeight = 80;
        pane.RowDefinitions[3].MinHeight = 100;
        pane.Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_PARAMETERS"), FontSize = 18, Margin = new Thickness(12) });
        parameterList.SelectionChanged += (_, _) =>
        {
            if (refreshingParameters)
                return;
            selectedParameterId = (parameterList.SelectedItem as ListBoxItem)?.Tag as string;
            refreshParameterDetails();
        };
        parameterList.ContextRequested += onParameterContextRequested;
        parameterList.KeyDown += (_, args) =>
        {
            if (args.Key != Avalonia.Input.Key.Delete || selectedParameterId is null)
                return;
            deleteParameter(selectedParameterId);
            args.Handled = true;
        };
        Grid.SetRow(parameterList, 1);
        pane.Children.Add(parameterList);
        GridSplitter splitter = new()
        {
            Height = 5, HorizontalAlignment = HorizontalAlignment.Stretch,
            ResizeDirection = GridResizeDirection.Rows, Background = EditorTheme.Brush("Border"),
        };
        Grid.SetRow(splitter, 2);
        pane.Children.Add(splitter);
        ScrollViewer details = scroll(parameterDetails);
        Grid.SetRow(details, 3);
        pane.Children.Add(details);
        ScrollViewer rendering = scroll(renderingProperties);
        rendering.MaxHeight = 180;
        Expander settings = new()
        {
            Header = LocaleService.Get("MATERIAL_RENDERING"), IsExpanded = false,
            HorizontalAlignment = HorizontalAlignment.Stretch, Content = rendering,
        };
        Grid.SetRow(settings, 4);
        pane.Children.Add(settings);
        return pane;
    }

    private void onParameterContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        bool byPointer = args.TryGetPosition(parameterList, out _);
        ListBoxItem? item = byPointer
            ? args.Source as ListBoxItem ?? (args.Source as Visual)?.GetVisualAncestors().OfType<ListBoxItem>().FirstOrDefault()
            : parameterList.SelectedItem as ListBoxItem;
        string? id = item?.Tag as string;
        if (id is not null)
            parameterList.SelectedItem = item;
        parameterList.Focus();
        ContextMenu menu = new() { Placement = byPointer ? PlacementMode.Pointer : PlacementMode.Bottom };
        MenuItem add = new() { Header = LocaleService.Get("MATERIAL_ADD_PARAMETER") };
        add.Click += async (_, _) => await createParameterAsync();
        menu.Items.Add(add);
        if (id is not null)
        {
            MenuItem remove = new() { Header = LocaleService.Get("DELETE") };
            remove.Click += (_, _) => deleteParameter(id);
            menu.Items.Add(remove);
        }
        args.Handled = true;
        menu.Open(parameterList);
    }

    private async Task createParameterAsync()
    {
        MaterialParameterCreationWindow dialog = new(registry!, id =>
            ((JsonArray)data["parameters"]!).OfType<JsonObject>().All(parameter => parameter["id"]!.GetValue<string>() != id));
        JsonObject? parameter = await dialog.ShowDialog<JsonObject?>(this);
        if (parameter is null)
            return;
        FlushPendingChanges();
        ((JsonArray)data["parameters"]!).Add(parameter);
        selectedParameterId = parameter["id"]!.GetValue<string>();
        commit();
        refreshParameters();
    }

    private void deleteParameter(string id)
    {
        JsonArray parameters = (JsonArray)data["parameters"]!;
        JsonObject? parameter = parameters.OfType<JsonObject>().FirstOrDefault(item => item["id"]!.GetValue<string>() == id);
        if (parameter is null)
            return;
        FlushPendingChanges();
        int index = parameters.IndexOf(parameter);
        parameters.RemoveAt(index);
        selectedParameterId = parameters.Count == 0 ? null : parameters[System.Math.Min(index, parameters.Count - 1)]!["id"]!.GetValue<string>();
        commit();
        refreshParameters();
    }

    private void refreshParameters(bool refreshDetails = true)
    {
        refreshingParameters = true;
        JsonObject[] parameters = ((JsonArray)data["parameters"]!).OfType<JsonObject>().ToArray();
        ListBoxItem[] items = parameters.Select(parameter =>
        {
            string id = parameter["id"]!.GetValue<string>();
            Grid row = new() { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
            row.Children.Add(new TextBlock { Text = parameter["displayName"]?.GetValue<string>() ?? id,
                TextTrimming = TextTrimming.CharacterEllipsis, VerticalAlignment = VerticalAlignment.Center });
            TextBlock type = new() { Text = parameter["type"]!.GetValue<string>(),
                Foreground = EditorTheme.Brush("TextMuted"), Margin = new Thickness(8, 0, 0, 0) };
            Grid.SetColumn(type, 1);
            row.Children.Add(type);
            ListBoxItem item = new() { Tag = id, Content = row, HorizontalContentAlignment = HorizontalAlignment.Stretch };
            ToolTip.SetTip(item, id + (parameter["group"]?.GetValue<string>() is string group && group.Length > 0 ? " · " + group : string.Empty));
            return item;
        }).ToArray();
        parameterList.ItemsSource = items;
        parameterList.SelectedItem = items.FirstOrDefault(item => (string)item.Tag! == selectedParameterId) ?? items.FirstOrDefault();
        selectedParameterId = (parameterList.SelectedItem as ListBoxItem)?.Tag as string;
        refreshingParameters = false;
        if (refreshDetails)
            refreshParameterDetails();
    }

    private void refreshParameterDetails()
    {
        clearForms(parameterDefault);
        clearForms(parameterDetails);
        JsonArray parameters = (JsonArray)data["parameters"]!;
        JsonObject? parameter = parameters.OfType<JsonObject>().FirstOrDefault(item => item["id"]!.GetValue<string>() == selectedParameterId);
        if (parameter is null)
        {
            parameterDetails.Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_NO_PARAMETERS"), TextWrapping = TextWrapping.Wrap });
            return;
        }
        parameterDetails.Children.Add(EditorInputs.CreateReadOnlyTextBox(parameter["id"]!.GetValue<string>()));
        parameterDetails.Children.Add(EditorInputs.CreateReadOnlyTextBox(parameter["type"]!.GetValue<string>()));
        foreach ((string id, string label) in new[] { ("displayName", "MATERIAL_PARAMETER_DISPLAY_NAME"), ("group", "MATERIAL_PARAMETER_GROUP") })
        {
            addForm(parameterDetails, new JsonObject { ["id"] = id, ["type"] = "string", ["displayName"] = LocaleService.Get(label) },
                parameter[id] ?? JsonValue.Create(string.Empty), value =>
                {
                    parameter[id] = value;
                    commit();
                    refreshParameters(false);
                });
        }
        parameterDetails.Children.Add(new TextBlock { Text = LocaleService.Get("DEFAULT_VALUE") });
        parameterDetails.Children.Add(parameterDefault);
        refreshParameterDefault(parameter);
        addParameterRange(parameter);
    }

    private void refreshParameterDefault(JsonObject parameter)
    {
        clearForms(parameterDefault);
        addForm(parameterDefault, parameter, parameter["default"], value => { parameter["default"] = value; commit(); });
    }

    private void addParameterRange(JsonObject parameter)
    {
        if (!((JsonArray)registry!["rangeParameterTypes"]!).Any(type => type?.GetValue<string>() == parameter["type"]?.GetValue<string>()))
            return;
        CheckBox range = new() { Content = LocaleService.Get("MATERIAL_RANGE"), IsChecked = parameter.ContainsKey("min") && parameter.ContainsKey("max") };
        StackPanel limits = new() { Spacing = 6, IsVisible = range.IsChecked == true };
        NumericUpDown minimum = EditorInputs.CreateNumericUpDown(JsonScalar.Decimal(parameter["min"]), decimal.MinValue, decimal.MaxValue, 0.1m);
        NumericUpDown maximum = EditorInputs.CreateNumericUpDown(JsonScalar.Decimal(parameter["max"], 1), decimal.MinValue, decimal.MaxValue, 0.1m);
        TextBlock error = new() { Text = LocaleService.Get("MATERIAL_RANGE_INVALID"), TextWrapping = TextWrapping.Wrap, IsVisible = false };
        HistoryMergeBehavior.Attach(minimum, gameData);
        HistoryMergeBehavior.Attach(maximum, gameData);
        limits.DetachedFromVisualTree += (_, _) =>
        {
            HistoryMergeBehavior.Detach(minimum);
            HistoryMergeBehavior.Detach(maximum);
        };
        void updateRange()
        {
            limits.IsVisible = range.IsChecked == true;
            error.IsVisible = range.IsChecked == true && (minimum.Value is null || maximum.Value is null || minimum.Value > maximum.Value);
            if (error.IsVisible)
                return;
            FlushPendingChanges();
            if (range.IsChecked == true)
            {
                parameter["min"] = (double)minimum.Value!.Value;
                parameter["max"] = (double)maximum.Value!.Value;
            }
            else
            {
                parameter.Remove("min");
                parameter.Remove("max");
            }
            commit();
            refreshParameterDefault(parameter);
        }
        minimum.ValueChanged += (_, _) => updateRange();
        maximum.ValueChanged += (_, _) => updateRange();
        range.IsCheckedChanged += (_, _) => updateRange();
        limits.Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_RANGE_MINIMUM") });
        limits.Children.Add(minimum);
        limits.Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_RANGE_MAXIMUM") });
        limits.Children.Add(maximum);
        limits.Children.Add(error);
        parameterDetails.Children.Add(range);
        parameterDetails.Children.Add(limits);
    }
}
