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
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Classes.Add("dialog");
        cancel.Click += (_, _) => Close((bool?)null);
        Button confirm = new() { Content = LocaleService.Get("CONFIRM"), IsDefault = true };
        confirm.Classes.Add("dialog");
        confirm.Classes.Add("accent");
        confirm.Click += (_, _) => Close((bool?)scope.IsProject);
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { cancel, confirm },
        };
        Border footer = new()
        {
            Padding = new Thickness(20, 12),
            Child = buttons,
        };
        footer.Classes.Add("editor-panel-footer");
        Grid root = new() { RowDefinitions = new RowDefinitions("*,Auto") };
        Border body = new() { Padding = new Thickness(22, 20), Child = scope };
        root.Children.Add(body);
        Grid.SetRow(footer, 1);
        root.Children.Add(footer);
        Content = root;
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
