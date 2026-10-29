using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectAssetPaths
{
    internal const string Section = "AssetPaths";
    private readonly ProjectDataStore store;
    private readonly string root;
    private readonly StringComparer fileSystemComparer;
    private readonly Dictionary<Guid, PathSnapshot> pathsByDocument = [];
    private volatile PathSnapshot[] currentPaths = [];

    internal ProjectAssetPaths(ProjectDataStore store)
    {
        this.store = store;
        root = Path.Combine(store.ProjectPath, "Assets");
        fileSystemComparer = OperatingSystem.IsWindows()
            || FileSystemPathIdentity.RefersToSameEntry(root, Path.Combine(store.ProjectPath, "assets"))
                ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
        store.Documents.Changed += onDocumentsChanged;
        Refresh();
    }

    public event EventHandler? Changed;
    public bool HasPendingChanges => currentPaths.Any(document => document.Modified);

    public EditorDocument? GetDocument(string key) => store.Documents.Find(Section, key);

    public EditorDocument? GetDocumentByPath(string path)
    {
        string fullPath = Path.GetFullPath(path);
        return documents().FirstOrDefault(document => document.Path == fullPath);
    }

    public string ResolveReadPath(string path)
    {
        string fullPath = Path.GetFullPath(path);
        PathSnapshot[] paths = currentPaths;
        PathSnapshot? document = paths.FirstOrDefault(document => document.Path == fullPath);
        if (document is not null)
            return document.SavedPath;
        PathSnapshot? directory = paths.Where(document => document.Directory
                && isChild(document.Path, fullPath))
            .OrderByDescending(document => document.Path.Length).FirstOrDefault();
        return directory is null ? fullPath
            : Path.Combine(directory.SavedPath, Path.GetRelativePath(directory.Path, fullPath));
    }

    public string ResolveLogicalPath(string path)
    {
        string fullPath = Path.GetFullPath(path);
        PathSnapshot[] paths = currentPaths;
        PathSnapshot? document = paths.FirstOrDefault(document => document.SavedPath == fullPath);
        if (document is not null)
            return document.Path;
        PathSnapshot? directory = paths.Where(document => document.Directory
                && isChild(document.SavedPath, fullPath))
            .OrderByDescending(document => document.SavedPath.Length).FirstOrDefault();
        return directory is null ? fullPath
            : Path.Combine(directory.Path, Path.GetRelativePath(directory.SavedPath, fullPath));
    }

    public bool FileExists(string path)
    {
        string fullPath = Path.GetFullPath(path);
        PathSnapshot? document = currentPaths.FirstOrDefault(document => document.Path == fullPath);
        if (document is not null)
            return !document.Directory && File.Exists(document.SavedPath);
        return !isHidden(fullPath) && File.Exists(ResolveReadPath(fullPath));
    }

    public bool DirectoryExists(string path)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        PathSnapshot? document = currentPaths.FirstOrDefault(document => document.Path == fullPath);
        if (document is not null)
            return document.Directory && Directory.Exists(document.SavedPath);
        return !isHidden(fullPath) && Directory.Exists(ResolveReadPath(fullPath));
    }

    public bool TryResolveAsset(string? reference, out string path)
    {
        path = string.Empty;
        if (!GameAssetPath.TryToProjectFile(store.ProjectPath, reference, out string logical)
            || !FileExists(logical))
            return false;
        string physical = ResolveReadPath(logical);
        if (!GameAssetPath.TryFromProjectFile(store.ProjectPath, physical, out _))
            return false;
        path = physical;
        return true;
    }

    public bool TryFromProjectFile(string path, out string reference)
    {
        reference = string.Empty;
        string logical = Path.GetFullPath(path);
        if (!contains(logical) || !FileExists(logical)
            || !GameAssetPath.TryFromProjectFile(store.ProjectPath, ResolveReadPath(logical), out _))
            return false;
        string candidate = toReference(logical);
        if (!GameAssetPath.IsCanonical(candidate))
            return false;
        reference = candidate;
        return true;
    }

    public bool TryResolveSelectionDirectory(string? reference, out string directory)
    {
        directory = string.Empty;
        if (!GameAssetPath.TryGetDirectoryRelativePath(reference, out string relative))
            return false;
        string logical = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
        if (isHidden(logical) && !currentPaths.Any(entry => entry.Path == logical))
            return false;
        string physical = ResolveReadPath(logical);
        string physicalReference = physical == root ? GameAssetPath.Root : toReference(physical);
        if (!GameAssetPath.TryResolveSelectionDirectory(store.ProjectPath, physicalReference, out _))
            return false;
        directory = logical;
        return true;
    }

    public bool TryResolveBaseHint(string? hint, out string directory)
    {
        directory = string.Empty;
        if (!GameAssetPath.IsValidBaseHint(hint))
            return false;
        string reference = string.IsNullOrEmpty(hint) ? GameAssetPath.Root
            : hint.StartsWith("/", StringComparison.Ordinal) ? hint : GameAssetPath.Root + "/" + hint;
        return TryResolveSelectionDirectory(reference, out directory);
    }

    public string ResolvePreviewAsset(string reference)
    {
        return TryResolveAsset(reference, out string physical)
            && GameAssetPath.TryFromProjectFile(store.ProjectPath, physical, out string saved)
                ? saved : reference;
    }

    internal IReadOnlyDictionary<string, string> CapturePreviewMapping()
    {
        return currentPaths.Where(entry => !entry.Directory && entry.Path != entry.SavedPath)
            .Select(entry => (Current: toReference(entry.Path), Saved: toReference(entry.SavedPath)))
            .Where(entry => GameAssetPath.IsCanonical(entry.Current) && GameAssetPath.IsCanonical(entry.Saved))
            .ToDictionary(entry => entry.Current, entry => entry.Saved, StringComparer.Ordinal);
    }

    public IReadOnlyList<ProjectAssetPathEntry> EnumerateEntries(string directory)
    {
        string logicalDirectory = Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory));
        Dictionary<string, ProjectAssetPathEntry> result = new(StringComparer.Ordinal);
        string physical = ResolveReadPath(logicalDirectory);
        if (DirectoryExists(logicalDirectory))
        {
            foreach (FileSystemInfo entry in new DirectoryInfo(physical).EnumerateFileSystemInfos())
            {
                if (EditorPathSandbox.IsLink(entry))
                    continue;
                string logical = ResolveLogicalPath(entry.FullName);
                if (Path.GetDirectoryName(logical) != logicalDirectory)
                    continue;
                result[logical] = createEntry(logical, entry);
            }
        }
        foreach (PathSnapshot document in currentPaths.Where(document => Path.GetDirectoryName(document.Path) == logicalDirectory))
        {
            FileSystemInfo entry = document.Directory ? new DirectoryInfo(document.SavedPath) : new FileInfo(document.SavedPath);
            if (entry.Exists)
                result[document.Path] = createEntry(document.Path, entry);
        }
        return result.Values.OrderBy(entry => !entry.IsDirectory)
            .ThenBy(entry => entry.Path, StringComparer.Ordinal).ToArray();
    }

    public void AssertCanWrite(IEnumerable<string> paths)
    {
        string[] requested = paths.Select(path => Path.TrimEndingDirectorySeparator(Path.GetFullPath(path))).ToArray();
        foreach (PathSnapshot document in currentPaths.Where(document => document.Modified))
            if (requested.Any(path => overlapsFileSystemPath(path, document.Path) || overlapsFileSystemPath(path, document.SavedPath)))
                throw new InvalidOperationException(LocaleService.Get("ASSET_PATHS_SAVE_REQUIRED"));
    }

    public Task<bool> MoveAsync(IReadOnlyList<(string OldPath, string NewPath)> moves,
        IProgress<EditorOperationProgress>? progress = null)
    {
        return store.RunAssetPathOperationAsync(captured => captured.AssetPaths.move(moves), progress);
    }

    private IEnumerable<EditorDocument> documents() => store.Documents.All.Where(document => document.Section == Section && document.Exists);

    private bool isHidden(string path)
    {
        return currentPaths.Any(document => document.Path != document.SavedPath
            && (document.SavedPath == path || document.Directory && isChild(document.SavedPath, path)));
    }

    private bool contains(string path) => isChild(root, path);
    private string toReference(string path) => GameAssetPath.Root + "/" + Path.GetRelativePath(root, path).Replace('\\', '/');
    private string key(string path) => Path.GetRelativePath(root, path).Replace('\\', '/');
    private static bool isDirectory(EditorDocument document) => document.InternalData?["directory"]?.GetValue<bool>() == true;
    private static bool isChild(string parent, string path) => path.StartsWith(Path.TrimEndingDirectorySeparator(parent) + Path.DirectorySeparatorChar, StringComparison.Ordinal);
    private bool overlapsFileSystemPath(string left, string right)
    {
        StringComparison comparison = fileSystemComparer == StringComparer.OrdinalIgnoreCase
            ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        return fileSystemComparer.Equals(left, right)
            || right.StartsWith(left + Path.DirectorySeparatorChar, comparison)
            || left.StartsWith(right + Path.DirectorySeparatorChar, comparison);
    }

    private static ProjectAssetPathEntry createEntry(string path, FileSystemInfo entry)
        => new(path, entry.FullName, entry is DirectoryInfo, entry is FileInfo file ? file.Length : 0, entry.LastWriteTimeUtc);

    internal void Refresh()
    {
        pathsByDocument.Clear();
        foreach (EditorDocument document in documents())
            pathsByDocument.Add(document.Id, createSnapshot(document));
        publishPaths();
    }

    private void publishPaths()
    {
        PathSnapshot[] paths = pathsByDocument.Values.ToArray();
        bool changed = !currentPaths.SequenceEqual(paths);
        currentPaths = paths;
        if (changed)
            Changed?.Invoke(this, EventArgs.Empty);
    }

    private void onDocumentsChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset)
        {
            Refresh();
            return;
        }
        bool changed = false;
        foreach (EditorDocument document in args.AffectedDocuments)
        {
            if (document.Section != Section)
                continue;
            if (!document.Exists || !store.Documents.IsRegistered(document))
            {
                changed |= pathsByDocument.Remove(document.Id);
                continue;
            }
            PathSnapshot snapshot = createSnapshot(document);
            if (pathsByDocument.TryGetValue(document.Id, out PathSnapshot? previous) && previous == snapshot)
                continue;
            pathsByDocument[document.Id] = snapshot;
            changed = true;
        }
        if (changed)
            publishPaths();
    }

    private static PathSnapshot createSnapshot(EditorDocument document)
        => new(document.Path, document.SavedPath, isDirectory(document), document.IsModified);

    private sealed record PathSnapshot(string Path, string SavedPath, bool Directory, bool Modified);
}
