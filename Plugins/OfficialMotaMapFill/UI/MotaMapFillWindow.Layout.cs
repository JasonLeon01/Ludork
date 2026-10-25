using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Plugin.Avalonia;

namespace Ludork.Plugins.OfficialMotaMapFill.UI;

internal sealed partial class MotaMapFillWindow
{
    private readonly StackPanel firstStep = new() { Spacing = 10 };
    private readonly Grid secondStep = new() { RowDefinitions = new RowDefinitions("Auto,Auto,*,Auto,Auto"), RowSpacing = 10 };

    private Control createLayout()
    {
        Grid maps = new() { RowDefinitions = new RowDefinitions("Auto,*"), Margin = new Thickness(12), RowSpacing = 10 };
        maps.Children.Add(label("mapList"));
        Grid.SetRow(mapList, 1);
        maps.Children.Add(mapList);

        Grid preview = new() { RowDefinitions = new RowDefinitions("Auto,Auto,*"), Margin = new Thickness(12), RowSpacing = 10 };
        preview.Children.Add(new DockPanel
        {
            LastChildFill = true,
            Children = { fullMap, label("mapPreview") },
        });
        DockPanel.SetDock(fullMap, Dock.Right);
        Grid.SetRow(regionText, 1);
        preview.Children.Add(regionText);
        ScrollViewer mapScroll = new()
        {
            Content = mapCanvas,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        Grid.SetRow(mapScroll, 2);
        preview.Children.Add(mapScroll);

        firstStep.Children.Add(label("selectLayer"));
        firstStep.Children.Add(layerSelector);
        firstStep.Children.Add(label("referenceSave"));
        firstStep.Children.Add(savePath);
        firstStep.Children.Add(new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Children = { browse, reload },
        });
        firstStep.Children.Add(label("selectPlayer"));
        firstStep.Children.Add(playerSelector);
        firstStep.Children.Add(attributes);
        firstStep.Children.Add(hint("saveHint"));
        firstStep.Children.Add(hint("replaceHint"));
        firstStep.Children.Add(next);

        secondStep.Children.Add(chooseBlueprints);
        Grid blueprintPreview = new() { ColumnDefinitions = new ColumnDefinitions("96,*"), ColumnSpacing = 10 };
        blueprintPreview.Children.Add(blueprintCanvas);
        Grid.SetColumn(blueprintLabel, 1);
        blueprintPreview.Children.Add(blueprintLabel);
        Grid.SetRow(blueprintPreview, 3);
        secondStep.Children.Add(blueprintPreview);
        Grid.SetRow(counts, 1);
        secondStep.Children.Add(counts);
        Grid.SetRow(categories, 2);
        secondStep.Children.Add(categories);
        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { back, generate },
        };
        Grid.SetRow(actions, 4);
        secondStep.Children.Add(actions);

        Grid steps = new();
        steps.Children.Add(new ScrollViewer { Content = firstStep, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled });
        steps.Children.Add(secondStep);
        Grid side = new() { RowDefinitions = new RowDefinitions("*,Auto"), Margin = new Thickness(14), RowSpacing = 12 };
        side.Children.Add(steps);
        Border statusPanel = new()
        {
            Padding = new Thickness(12),
            Background = PluginTheme.Brush("Surface"),
            Child = new StackPanel
            {
                Spacing = 8,
                Children =
                {
                    new ScrollViewer
                    {
                        MaxHeight = 150,
                        Content = status,
                        HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
                        VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
                    },
                    progress,
                },
            },
        };
        Grid.SetRow(statusPanel, 1);
        side.Children.Add(statusPanel);

        Grid root = new() { ColumnDefinitions = new ColumnDefinitions("220,5,*,5,360") };
        root.Children.Add(maps);
        GridSplitter left = splitter();
        Grid.SetColumn(left, 1);
        root.Children.Add(left);
        Grid.SetColumn(preview, 2);
        root.Children.Add(preview);
        GridSplitter right = splitter();
        Grid.SetColumn(right, 3);
        root.Children.Add(right);
        Grid.SetColumn(side, 4);
        root.Children.Add(side);
        return root;
    }

    private void showStep(bool actors)
    {
        choosingActors = actors;
        firstStep.IsVisible = !actors;
        secondStep.IsVisible = actors;
        if (actors)
        {
            rebuildChoices();
            status.Text = localizer.Text("chooseBlueprintsHint");
        }
        updateControls();
    }

    private TextBlock label(string key) => new()
    {
        Text = localizer.Text(key),
        FontWeight = FontWeight.SemiBold,
        VerticalAlignment = VerticalAlignment.Center,
    };

    private TextBlock hint(string key) => new()
    {
        Text = localizer.Text(key),
        Foreground = PluginTheme.Brush("TextMuted"),
        TextWrapping = TextWrapping.Wrap,
    };

    private static GridSplitter splitter() => new()
    {
        Width = 5,
        HorizontalAlignment = HorizontalAlignment.Stretch,
        Background = PluginTheme.Brush("Border"),
    };
}
