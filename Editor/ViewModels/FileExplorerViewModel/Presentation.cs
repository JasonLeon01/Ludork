using Avalonia;
using Ludork.Services;
using System;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private double zoom = 100;

    public double Zoom
    {
        get => zoom;
        set
        {
            double next = double.IsFinite(value) ? Math.Clamp(value, 50, 200) : 100;
            if (!SetProperty(ref zoom, next))
                return;
            OnPropertyChanged(nameof(ZoomLabel));
            OnPropertyChanged(nameof(IconSize));
            OnPropertyChanged(nameof(ListIconSize));
            OnPropertyChanged(nameof(EntryFontSize));
            OnPropertyChanged(nameof(PathFontSize));
            OnPropertyChanged(nameof(ModifiedFontSize));
            OnPropertyChanged(nameof(IconTileWidth));
            OnPropertyChanged(nameof(IconTileHeight));
            OnPropertyChanged(nameof(IconContentWidth));
            OnPropertyChanged(nameof(IconMargin));
            OnPropertyChanged(nameof(IconItemMargin));
            OnPropertyChanged(nameof(ListMargin));
            OnPropertyChanged(nameof(ListItemMargin));
            OnPropertyChanged(nameof(IconSpacing));
            OnPropertyChanged(nameof(ListSpacing));
            OnPropertyChanged(nameof(TreeExpanderSize));
            OnPropertyChanged(nameof(TreeExpanderIconSize));
            foreach (FileExplorerEntryViewModel entry in Entries)
                entry.UpdateIndentation(next);
            if (EditorLayoutService.Settings is EditorSettings settings)
            {
                settings.FileExplorerZoom = next;
                EditorLayoutService.Save();
            }
        }
    }

    public string ZoomLabel => $"{Zoom:0}%";
    public string ZoomHint => LocaleService.Get(OperatingSystem.IsMacOS()
        ? "FILE_EXPLORER_ZOOM_MAC" : "FILE_EXPLORER_ZOOM");
    public double IconSize => 64 * Zoom / 100;
    public double ListIconSize => 28 * Zoom / 100;
    public double EntryFontSize => 13 * Zoom / 100;
    public double PathFontSize => 11 * Zoom / 100;
    public double ModifiedFontSize => 18 * Zoom / 100;
    public double IconTileWidth => 162 * Zoom / 100;
    public double IconTileHeight => (IsSearching ? 142 : 124) * Zoom / 100;
    public double IconContentWidth => 134 * Zoom / 100;
    public Thickness IconMargin => new(6 * Zoom / 100);
    public Thickness IconItemMargin => new(8 * Zoom / 100);
    public Thickness ListMargin => new(6 * Zoom / 100, 3 * Zoom / 100);
    public Thickness ListItemMargin => new(8 * Zoom / 100, 4 * Zoom / 100);
    public double IconSpacing => 3 * Zoom / 100;
    public double ListSpacing => 8 * Zoom / 100;
    public double TreeExpanderSize => 16 * Zoom / 100;
    public double TreeExpanderIconSize => 12 * Zoom / 100;
}
