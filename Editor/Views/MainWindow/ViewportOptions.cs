using Avalonia;
using Avalonia.Automation;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System;

namespace Ludork.Views;

public partial class MainWindow
{
    private void onViewportOptionsClick(object? sender, RoutedEventArgs args)
    {
        if (sender is Control target)
            showViewportOptions(target);
    }

    private void showViewportOptions(Control target)
    {
        if (viewModel is null)
            return;
        MapPreviewService service = viewModel.BlueprintPreviews.MapPreviews;
        MapPreviewOptions options = service.Options;
        StackPanel content = new() { Spacing = 8, MinWidth = 210 };
        content.Children.Add(new TextBlock { Text = LocaleService.Get("MAP_VIEW_MODE"), FontWeight = Avalonia.Media.FontWeight.Bold });
        RadioButton unlit = new() { Content = LocaleService.Get("MAP_VIEW_UNLIT"), GroupName = "MapViewMode", IsChecked = !options.Lit };
        RadioButton lit = new() { Content = LocaleService.Get("MAP_VIEW_LIT"), GroupName = "MapViewMode", IsChecked = options.Lit };
        content.Children.Add(unlit);
        content.Children.Add(lit);
        content.Children.Add(new Separator());
        addViewportOption(content, "MAP_VIEW_REALTIME", options.Realtime, value => service.SetOptions(service.Options with { Realtime = value }));
        addViewportOption(content, "MAP_VIEW_FOG", options.ShowFog, value => service.SetOptions(service.Options with { ShowFog = value }));
        addViewportOption(content, "MAP_VIEW_PANORAMA", options.ShowPanorama, value => service.SetOptions(service.Options with { ShowPanorama = value }));
        addViewportOption(content, "MAP_VIEW_ACTOR_ORIGINS", options.ShowActorOrigins, value => service.SetOptions(service.Options with { ShowActorOrigins = value }));
        CheckBox lightBounds = addViewportOption(content, "MAP_VIEW_LIGHT_BOUNDS", options.ShowLightBounds,
            value => service.SetOptions(service.Options with { ShowLightBounds = value }));
        lightBounds.IsEnabled = !options.Lit;
        unlit.IsCheckedChanged += (_, _) =>
        {
            if (unlit.IsChecked == true)
            {
                service.SetOptions(service.Options with { Lit = false });
                lightBounds.IsEnabled = true;
            }
        };
        lit.IsCheckedChanged += (_, _) =>
        {
            if (lit.IsChecked == true)
            {
                service.SetOptions(service.Options with { Lit = true });
                lightBounds.IsEnabled = false;
            }
        };
        new Flyout { Content = content }.ShowAt(target);
    }

    private static CheckBox addViewportOption(StackPanel panel, string key, bool value, Action<bool> change)
    {
        CheckBox checkBox = new() { Content = LocaleService.Get(key), IsChecked = value };
        checkBox.IsCheckedChanged += (_, _) => change(checkBox.IsChecked == true);
        panel.Children.Add(checkBox);
        return checkBox;
    }
}
