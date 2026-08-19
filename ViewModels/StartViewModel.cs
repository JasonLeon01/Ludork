using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.ViewModels;

public sealed class StartViewModel : ViewModelBase
{
    public StartViewModel(EditorSettings editorSettings)
    {
        RecentProjects = editorSettings.RecentProjectPaths
            .Select(projectFilePath => new RecentProjectViewModel(projectFilePath))
            .ToArray();
    }

    public string AppName => "Ludork";
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
    public IReadOnlyList<RecentProjectViewModel> RecentProjects { get; }
    public bool HasRecentProjects => RecentProjects.Count > 0;
    public bool HasNoRecentProjects => RecentProjects.Count == 0;
}

public sealed class RecentProjectViewModel
{
    public RecentProjectViewModel(string projectFilePath)
    {
        ProjectFilePath = projectFilePath;
        ProjectDirectory = Path.GetDirectoryName(projectFilePath)
            ?? throw new ArgumentException("Project file path has no parent directory.", nameof(projectFilePath));
        string directoryName = Path.GetFileName(ProjectDirectory.TrimEnd(Path.DirectorySeparatorChar));
        Name = string.IsNullOrWhiteSpace(directoryName)
            ? Path.GetFileNameWithoutExtension(projectFilePath)
            : directoryName;
    }

    public string Name { get; }
    public string ProjectDirectory { get; }
    public string ProjectFilePath { get; }
}
