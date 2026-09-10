using System;
using System.IO;

namespace Ludork.ViewModels;

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
