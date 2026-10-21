using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record PreparedResourceDeletion(
    string Section,
    IReadOnlyList<PreparedResourceDeletion.Target> Targets,
    IReadOnlyList<string> SavedPaths,
    EditorFileSaveBatch Files)
{
    internal sealed record Target(string Section, string Key, string Path, string SavedKey, string SavedPath);
}
