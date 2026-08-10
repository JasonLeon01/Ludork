using Ludork.Services;

namespace Ludork.ViewModels;

public sealed class StartViewModel : ViewModelBase
{
    public string AppName => "Ludork";
    public string NewProject => LocaleService.Get("NEW_PROJECT");
    public string OpenProject => LocaleService.Get("OPEN_PROJECT");
    public string Plugins => LocaleService.Get("PLUGINS");
    public string ImportPlugin => LocaleService.Get("IMPORT_PLUGIN");
    public string ManagePlugins => LocaleService.Get("MANAGE_PLUGINS");
}
