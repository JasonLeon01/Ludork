using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Ludork.ViewModels;

namespace Ludork.Views.Utils;

internal static class ProjectWindowContext
{
    public static MainViewModel? Find(Visual? visual)
    {
        TopLevel? root = TopLevel.GetTopLevel(visual);
        while (root is PopupRoot popup)
            root = popup.ParentTopLevel;
        for (Window? window = root as Window; window is not null; window = window.Owner as Window)
        {
            if (window.DataContext is MainViewModel project)
                return project;
        }
        return null;
    }
}
