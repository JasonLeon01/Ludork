using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Controls.Primitives;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialRandomMap.Localization;
using System;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class TilesetSelectionDialog : Window
{
    private readonly TilesetGridControl grid;
    private readonly Button confirmButton = new();
    private readonly TaskCompletionSource<int?> completion =
        new(TaskCreationOptions.RunContinuationsAsynchronously);

    private TilesetSelectionDialog(
        IMapEditorHost host,
        PluginTilesetSnapshot tileset,
        int? selectedTile,
        PluginLocalizer localizer)
    {
        Title = localizer.Text("tileDialogTitle");
        Width = 800;
        Height = 680;
        MinWidth = 520;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#161718"));

        grid = new TilesetGridControl(host, tileset, selectedTile);
        grid.SelectionChanged += (_, _) =>
            confirmButton.IsEnabled = grid.SelectedTile is not null;
        ScrollViewer scroll = new()
        {
            Content = grid,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        TextBlock hint = new()
        {
            Text = localizer.Text(
                OperatingSystem.IsMacOS()
                    ? "tileDialogHintMac"
                    : "tileDialogHint"),
            Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
            TextWrapping = TextWrapping.Wrap,
        };
        Button cancelButton = new()
        {
            Content = localizer.Text("cancel"),
            MinWidth = 88,
        };
        confirmButton.Content = localizer.Text("confirm");
        confirmButton.MinWidth = 88;
        confirmButton.IsEnabled = selectedTile is not null;
        cancelButton.Click += (_, _) =>
        {
            completion.TrySetResult(null);
            Close();
        };
        confirmButton.Click += (_, _) =>
        {
            completion.TrySetResult(grid.SelectedTile);
            Close();
        };
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(cancelButton);
        buttons.Children.Add(confirmButton);
        Grid root = new()
        {
            Margin = new Thickness(14),
            RowDefinitions = new RowDefinitions("Auto,*,Auto"),
            RowSpacing = 10,
        };
        root.Children.Add(hint);
        Grid.SetRow(scroll, 1);
        root.Children.Add(scroll);
        Grid.SetRow(buttons, 2);
        root.Children.Add(buttons);
        Content = root;
        Closed += (_, _) =>
        {
            completion.TrySetResult(null);
            grid.Dispose();
        };
    }

    public static async Task<int?> ShowAsync(
        Window owner,
        IMapEditorHost host,
        PluginTilesetSnapshot tileset,
        int? selectedTile,
        PluginLocalizer localizer)
    {
        TilesetSelectionDialog dialog = new(
            host,
            tileset,
            selectedTile,
            localizer);
        Task lifetime = dialog.ShowDialog(owner);
        int? result = await dialog.completion.Task;
        await lifetime;
        return result;
    }
}
