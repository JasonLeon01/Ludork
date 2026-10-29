using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed partial class MaterialWindow
{
    private void buildContent()
    {
        Grid body = new() { ColumnDefinitions = new ColumnDefinitions("280,5,*") };
        body.ColumnDefinitions[0].MinWidth = 220;
        body.ColumnDefinitions[2].MinWidth = 380;
        body.Children.Add(buildParameterPane());
        GridSplitter splitter = columnSplitter();
        Grid.SetColumn(splitter, 1);
        body.Children.Add(splitter);

        Grid pages = new() { RowDefinitions = new RowDefinitions("*,Auto") };
        tabs.Items.Add(new TabItem { Header = LocaleService.Get("PREVIEW"), Content = buildPreviewPage() });
        tabs.Items.Add(new TabItem { Header = LocaleService.Get("MATERIAL_NODE_GRAPH"), Content = buildGraphPage() });
        tabs.SelectedIndex = 0;
        pages.Children.Add(tabs);
        ScrollViewer errors = scroll(diagnostics);
        errors.MaxHeight = 160;
        errors.Margin = new Thickness(8);
        Grid.SetRow(errors, 1);
        pages.Children.Add(errors);
        Grid.SetColumn(pages, 2);
        body.Children.Add(pages);
        Content = body;
    }

    private Control buildPreviewPage()
    {
        DockPanel page = new() { LastChildFill = true };
        Button texture = new() { Content = LocaleService.Get("MATERIAL_PREVIEW_TEXTURE"), Margin = new Thickness(8) };
        texture.Click += async (_, _) =>
        {
            string? selected = await FileSelectorDialog.ShowAsync(this, Path.Combine(gameData.ProjectPath, "Assets"),
                FileSelectorDialog.FilesFilter("*.png", "*.jpg", "*.jpeg"), allowEmpty: true);
            if (selected is not null)
            {
                texturePath = selected.Length == 0 ? string.Empty
                    : gameData.AssetPaths.TryFromProjectFile(selected, out string reference) ? reference : string.Empty;
                generation++;
            }
        };
        DockPanel.SetDock(texture, Dock.Top);
        page.Children.Add(texture);
        Grid frame = new() { ColumnDefinitions = new ColumnDefinitions("*,*,*,*"), ColumnSpacing = 8 };
        string[] fields = ["X", "Y", "Width", "Height"];
        for (int index = 0; index < fields.Length; index++)
        {
            int component = index;
            StackPanel field = new();
            addForm(field, new JsonObject { ["id"] = fields[index], ["type"] = "integer",
                ["min"] = index < 2 ? 0 : 1, ["max"] = int.MaxValue, ["slider"] = false }, frameRect[index],
                value => { frameRect[component] = value; generation++; });
            Grid.SetColumn(field, index);
            frame.Children.Add(field);
        }
        previewProperties.Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_PREVIEW_FRAME") });
        previewProperties.Children.Add(frame);
        addForm(previewProperties, new JsonObject { ["id"] = "ambient", ["type"] = "color" },
            new JsonArray(ambient.Select(channel => JsonValue.Create(JsonScalar.Number(channel) / 255) as JsonNode).ToArray()),
            value => { ambient = new JsonArray(((JsonArray)value!).Select(channel => JsonValue.Create(JsonScalar.Number(channel) * 255) as JsonNode).ToArray()); generation++; });
        CheckBox light = new() { Content = LocaleService.Get("MATERIAL_PREVIEW_LIGHT"), IsChecked = lightEnabled };
        light.IsCheckedChanged += (_, _) => { lightEnabled = light.IsChecked == true; generation++; };
        previewProperties.Children.Add(light);
        Expander settings = new()
        {
            Header = LocaleService.Get("MATERIAL_PREVIEW"), IsExpanded = true,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            Content = new ScrollViewer { Content = previewProperties, MaxHeight = 220 },
        };
        DockPanel.SetDock(settings, Dock.Bottom);
        page.Children.Add(settings);
        page.Children.Add(surface);
        return page;
    }

    private Control buildGraphPage()
    {
        DockPanel page = new();
        Button source = new() { Content = LocaleService.Get("MATERIAL_GENERATED_SHADER"), Margin = new Thickness(8) };
        source.Click += (_, _) =>
        {
            Window window = new()
            {
                Title = LocaleService.Get("MATERIAL_GENERATED_SHADER"), Width = 1000, Height = 700,
                Content = new TextBox { Text = generatedShader, IsReadOnly = true, AcceptsReturn = true,
                    FontFamily = new FontFamily("monospace"), TextWrapping = TextWrapping.NoWrap },
            };
            window.Show(this);
        };
        DockPanel.SetDock(source, Dock.Top);
        page.Children.Add(source);
        page.Children.Add(graph);
        return page;
    }

    private void refreshRenderingProperties()
    {
        clearForms(renderingProperties);
        JsonObject values = data["rendering"] as JsonObject ?? new JsonObject();
        foreach (JsonObject descriptor in ((JsonArray)registry!["renderingFields"]!).OfType<JsonObject>())
        {
            string id = descriptor["id"]!.GetValue<string>();
            addForm(renderingProperties, descriptor, values[id], value =>
            {
                if (data["rendering"] is not JsonObject)
                    data["rendering"] = values;
                values[id] = value;
                commitFields(values, id);
            });
        }
    }

    private static ScrollViewer scroll(Control content) => new()
    {
        Content = content, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
        VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
    };

    private static GridSplitter columnSplitter() => new()
    {
        Width = 5, HorizontalAlignment = HorizontalAlignment.Stretch,
        ResizeDirection = GridResizeDirection.Columns,
        Background = EditorTheme.Brush("Border"),
    };
}
