using Avalonia;
using Avalonia.Controls;
using Ludork.Plugin.Abstractions;
using System.Threading.Tasks;

namespace Ludork.Views.Coordination;

internal sealed class PluginMenuCoordinator
{
    private readonly Window owner;
    private readonly string projectPath;

    public PluginMenuCoordinator(Window owner, string projectPath)
    {
        this.owner = owner;
        this.projectPath = projectPath;
        install();
    }

    public async Task ImportAsync()
    {
        if (Application.Current is App app)
            await app.importPluginAsync(owner);
    }

    public async Task ManageAsync()
    {
        if (Application.Current is App app)
            await app.showPluginManagerAsync(owner);
    }

    private void install()
    {
        NativeMenu? rootMenu = NativeMenu.GetMenu(owner);
        if (Application.Current is not App app
            || rootMenu is null
            || rootMenu.Items.Count < 6
            || rootMenu.Items[0] is not NativeMenuItem { Menu: NativeMenu fileMenu }
            || rootMenu.Items[1] is not NativeMenuItem { Menu: NativeMenu editMenu }
            || rootMenu.Items[2] is not NativeMenuItem { Menu: NativeMenu gameMenu }
            || rootMenu.Items[3] is not NativeMenuItem { Menu: NativeMenu databaseMenu }
            || rootMenu.Items[4] is not NativeMenuItem { Menu: NativeMenu pluginsMenu }
            || rootMenu.Items[5] is not NativeMenuItem { Menu: NativeMenu helpMenu })
        {
            return;
        }
        app.installPluginMenus(
            owner,
            projectPath.Length == 0 ? null : projectPath,
            (PluginMenuLocation.File, fileMenu),
            (PluginMenuLocation.Edit, editMenu),
            (PluginMenuLocation.Game, gameMenu),
            (PluginMenuLocation.Database, databaseMenu),
            (PluginMenuLocation.Help, helpMenu),
            (PluginMenuLocation.Plugins, pluginsMenu));
    }

}
