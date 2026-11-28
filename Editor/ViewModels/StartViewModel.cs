using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.ViewModels;

public sealed class StartViewModel : ViewModelBase, IDisposable
{
    private readonly EditorSettings settings;

    public StartViewModel(EditorSettings editorSettings)
    {
        settings = editorSettings;
        settings.RecentProjectsChanged += onRecentProjectsChanged;
        LocaleService.LanguageChanged += onLanguageChanged;
    }

    public string AppName => "Ludork";
    public string CloseWindow => LocaleService.Get("CLOSE_WINDOW");
    public string StartTitle => LocaleService.Get("START_TITLE");
    public string StartSubtitle => LocaleService.Get("START_SUBTITLE");
    public string NewProject => LocaleService.Get("NEW_PROJECT");
    public string OpenProject => LocaleService.Get("OPEN_PROJECT");
    public string RecentProjectsTitle => LocaleService.Get("START_RECENT_PROJECTS");
    public string NoRecentProjects => LocaleService.Get("START_NO_RECENT_PROJECTS");
    public string DropProjectHint => LocaleService.Get("START_DROP_PROJECT_HINT");
    public string Plugins => LocaleService.Get("PLUGINS");
    public string ImportPlugin => LocaleService.Get("IMPORT_PLUGIN");
    public string ManagePlugins => LocaleService.Get("MANAGE_PLUGINS");
    public IReadOnlyList<RecentProjectViewModel> RecentProjects => settings.RecentProjectPaths
        .Select(path => new RecentProjectViewModel(path)).ToArray();
    public bool HasRecentProjects => settings.RecentProjectPaths.Count > 0;
    public bool HasNoRecentProjects => !HasRecentProjects;

    public void Dispose()
    {
        settings.RecentProjectsChanged -= onRecentProjectsChanged;
        LocaleService.LanguageChanged -= onLanguageChanged;
    }

    private void onLanguageChanged(object? sender, EventArgs args) => OnPropertyChanged(string.Empty);

    private void onRecentProjectsChanged(object? sender, EventArgs args)
    {
        OnPropertyChanged(nameof(RecentProjects));
        OnPropertyChanged(nameof(HasRecentProjects));
        OnPropertyChanged(nameof(HasNoRecentProjects));
    }
}
