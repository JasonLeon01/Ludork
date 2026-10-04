using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Ludork.Services;
using Ludork.Views.Utils;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed class PluginScopeDialog : Window
{
    private PluginScopeDialog()
    {
        Title = LocaleService.Get("IMPORT_PLUGIN");
        Width = 420;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);

        PluginScopeSelector scope = new(true);
        Button confirm = new() { Content = LocaleService.Get("CONFIRM"), IsDefault = true };
        confirm.Click += (_, _) => Close((bool?)scope.IsProject);
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close((bool?)null);
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };
        Content = new StackPanel
        {
            Margin = new Thickness(22),
            Spacing = 20,
            Children = { scope, buttons },
        };
        KeyDown += (_, args) =>
        {
            if (args.Key != Key.Escape)
                return;
            Close((bool?)null);
            args.Handled = true;
        };
    }

    public static Task<bool?> ShowAsync(Window owner)
    {
        return new PluginScopeDialog().ShowDialog<bool?>(owner);
    }
}
