using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;

namespace Ludork.Services.Plugins;

internal sealed class ProjectScriptInputs
{
    private readonly Dictionary<string, string> fingerprints = new(ProjectDiskSnapshot.PathComparer);

    public static ProjectScriptInputs Capture(string projectPath, string scriptPath, CancellationToken cancellationToken)
    {
        ProjectScriptInputs result = new();
        EnumerationOptions options = new()
        {
            RecurseSubdirectories = true,
            IgnoreInaccessible = false,
            AttributesToSkip = FileAttributes.ReparsePoint,
        };
        foreach (string section in new[] { "Data", "Scripts" })
        {
            string root = Path.Combine(projectPath, section);
            if (!Directory.Exists(root))
                continue;
            foreach (string path in Directory.EnumerateFiles(root, "*", options))
            {
                cancellationToken.ThrowIfCancellationRequested();
                string relative = Path.GetRelativePath(root, path).Replace('\\', '/');
                if (section == "Data" && relative.StartsWith("Maps/", StringComparison.Ordinal)
                    || section == "Scripts" && relative.StartsWith("stub/", StringComparison.Ordinal))
                    continue;
                result.capture(path);
            }
        }
        cancellationToken.ThrowIfCancellationRequested();
        result.capture(scriptPath);
        return result;
    }

    public bool Matches(ProjectScriptInputs other) => ProjectDiskSnapshot.Equal(fingerprints, other.fingerprints);

    private void capture(string path)
    {
        fingerprints[path] = ProjectDiskSnapshot.ReadFingerprint(path)
            ?? throw new IOException("A project evaluation input disappeared: " + path);
    }
}
