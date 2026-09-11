using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class ProjectExportService : IDisposable
{
    private const int RecordVersion = 1;
    private static readonly JsonSerializerOptions jsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
    };
    private readonly string projectPath;
    private readonly string recordPath;
    private readonly ProjectStateWorker stateWorker;
    private readonly IEditorPluginRuntime? pluginRuntime;
    private readonly ProjectOperationPipeline pipeline;
    private readonly SemaphoreSlim operationLock = new(1, 1);
    private readonly CancellationTokenSource lifetime = new();
    private readonly FileSystemWatcher watcher;
    private bool disposed;

    public ProjectExportService(string projectPath, ProjectStateWorker stateWorker, IEditorPluginRuntime? pluginRuntime = null)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        recordPath = Path.Combine(this.projectPath, ProjectToolConstants.EditorCacheDirectory, "ProjectExport.json");
        this.stateWorker = stateWorker;
        this.pluginRuntime = pluginRuntime ?? TextHintService.Runtime;
        pipeline = new ProjectOperationPipeline(this.projectPath, this.pluginRuntime);
        watcher = new FileSystemWatcher(this.projectPath)
        {
            IncludeSubdirectories = true,
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite,
        };
        watcher.Created += onFileChanged;
        watcher.Deleted += onFileChanged;
        watcher.Changed += onFileChanged;
        watcher.Renamed += onFileRenamed;
        watcher.Error += (_, _) => RefreshAvailability();
        watcher.EnableRaisingEvents = true;
        RefreshAvailability();
    }

    public bool HasSuccessfulExport { get; private set; }
    public string Detail { get; private set; } = string.Empty;
    public event EventHandler? Changed;

    public void RefreshAvailability()
    {
        if (disposed)
            return;
        bool available = readRecord() is not null;
        if (available == HasSuccessfulExport)
            return;
        HasSuccessfulExport = available;
        Changed?.Invoke(this, EventArgs.Empty);
    }

    public async Task<bool> CheckAsync(CancellationToken cancellationToken = default)
    {
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, lifetime.Token);
        await operationLock.WaitAsync(cancellation.Token);
        try
        {
            ExportRecord? record = readRecord();
            if (record is null || !record.Complete)
                return setResult(false, "Export the project before playing.");
            ExportSnapshot snapshot = await captureSnapshotAsync(cancellation.Token);
            bool current = record.Snapshot is not null && sameInputs(record.Snapshot, snapshot)
                && equalHashes(record.Snapshot.Outputs, snapshot.Outputs);
            return setResult(current, current ? string.Empty : "Export inputs or generated files have changed.");
        }
        catch (Exception exception) when (isExportException(exception))
        {
            return setResult(false, exception.Message);
        }
        finally
        {
            RefreshAvailability();
            operationLock.Release();
        }
    }

    public async Task<ProjectExportResult> ExportAsync(
        Action<string> writeOutput,
        CancellationToken cancellationToken = default)
    {
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, lifetime.Token);
        await operationLock.WaitAsync(cancellation.Token);
        try
        {
            ExportRecord? previous = readRecord();
            if (previous is not null)
                await writeRecordAsync(previous with { Complete = false }, cancellation.Token);
            ExportSnapshot before = await captureSnapshotAsync(cancellation.Token);
            PluginResult hooks = await pipeline.ExecuteAsync(ProjectOperationKind.Export, writeOutput, cancellation.Token);
            if (!hooks.Success)
                return ProjectExportResult.Failed(hooks.Error);
            SaveResult generation = await UiAssetGenerationService.ExecuteAsync(
                projectPath, "generate", writeOutput, cancellation.Token);
            if (!generation.Success)
                return ProjectExportResult.Failed(generation.Details);
            ExportSnapshot after = await captureSnapshotAsync(cancellation.Token);
            if (!sameInputs(before, after))
                return ProjectExportResult.Failed("Export inputs changed during export. Export the project again.");
            string[] missing = after.Outputs.Where(pair => pair.Value == "missing").Select(pair => pair.Key).ToArray();
            if (missing.Length != 0)
                return ProjectExportResult.Failed("Export outputs are missing: " + string.Join(", ", missing));
            cancellation.Token.ThrowIfCancellationRequested();
            await writeRecordAsync(new ExportRecord(RecordVersion, true, after), cancellation.Token);
            setResult(true, string.Empty);
            writeOutput(LocaleService.Get("EXPORT_COMPLETE"));
            return ProjectExportResult.Completed();
        }
        catch (OperationCanceledException)
        {
            return ProjectExportResult.CancelledResult();
        }
        catch (Exception exception) when (isExportException(exception) || exception is ProjectStateCheckException)
        {
            return ProjectExportResult.Failed(exception.Message);
        }
        finally
        {
            RefreshAvailability();
            operationLock.Release();
        }
    }

    private async Task<ExportSnapshot> captureSnapshotAsync(CancellationToken cancellationToken)
    {
        (JsonElement manifest, string toolIdentity) = await stateWorker.ExecuteAsync("ui-manifest", cancellationToken);
        return await Task.Run(() =>
        {
            SortedDictionary<string, string> inputs = new(StringComparer.Ordinal);
            foreach (JsonProperty property in manifest.GetProperty("inputs").EnumerateObject())
            {
                cancellationToken.ThrowIfCancellationRequested();
                resolveProjectFile(property.Name);
                inputs.Add(property.Name, property.Value.GetString() ?? throw new InvalidDataException(property.Name));
            }
            string[] uiOutputs = manifest.GetProperty("outputPaths").EnumerateArray()
                .Select(value => value.GetString() ?? throw new InvalidDataException("Invalid UI output path")).ToArray();
            IReadOnlyList<ProjectExportParticipant> participants = pluginRuntime?.GetExportParticipants(projectPath) ?? [];
            SortedDictionary<string, string> outputs = new(StringComparer.Ordinal);
            HashSet<string> outputOwners = new(StringComparer.OrdinalIgnoreCase);
            foreach (string path in uiOutputs)
            {
                if (!outputOwners.Add(path))
                    throw new InvalidDataException("Export outputs conflict on case-insensitive filesystems: " + path);
                outputs.Add(path, hashProjectFile(path, cancellationToken));
            }
            foreach (ProjectExportParticipant participant in participants)
            {
                foreach (string path in participant.Files.InputPaths)
                    inputs.TryAdd(path, hashProjectFile(path, cancellationToken));
                foreach (string path in participant.Files.OutputPaths)
                {
                    if (!outputOwners.Add(path))
                        throw new InvalidDataException("Export outputs have multiple owners: " + path);
                    outputs.Add(path, hashProjectFile(path, cancellationToken));
                }
            }
            if (inputs.Keys.Any(outputOwners.Contains))
                throw new InvalidDataException("Export inputs and outputs must be separate files.");
            string generatorIdentity = typeof(ProjectExportService).Module.ModuleVersionId + ":" + toolIdentity;
            return new ExportSnapshot(generatorIdentity, participants.Select(value => value.Identity).ToArray(), inputs, outputs);
        }, cancellationToken);
    }

    private string hashProjectFile(string relativePath, CancellationToken cancellationToken)
    {
        string path = resolveProjectFile(relativePath);
        if (Directory.Exists(path))
            throw new InvalidDataException("Export declarations must identify files: " + relativePath);
        return File.Exists(path) ? hashFile(path, cancellationToken) : "missing";
    }

    private string resolveProjectFile(string relativePath)
    {
        string[] parts = relativePath.Split('/');
        if (Path.IsPathRooted(relativePath) || relativePath.Contains('\\') || relativePath.Contains(':')
            || parts.Any(part => part.Length == 0 || part is "." or ".."))
            throw new InvalidDataException("Export paths must be project-relative file paths: " + relativePath);
        string current = projectPath;
        foreach (string part in parts)
        {
            current = Path.Combine(current, part);
            if (Path.Exists(current) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Export paths must not contain symbolic links: " + relativePath);
        }
        return current;
    }

    private static string hashFile(string path, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        using FileStream stream = File.OpenRead(path);
        string hash = Convert.ToHexStringLower(SHA256.HashData(stream));
        cancellationToken.ThrowIfCancellationRequested();
        return hash;
    }

    private ExportRecord? readRecord()
    {
        try
        {
            if (!File.Exists(recordPath))
                return null;
            ExportRecord? record = JsonSerializer.Deserialize<ExportRecord>(File.ReadAllText(recordPath), jsonOptions);
            return record is { FormatVersion: RecordVersion, Snapshot: not null }
                && record.Snapshot.GeneratorIdentity is not null && record.Snapshot.Participants is not null
                && record.Snapshot.Inputs is not null && record.Snapshot.Outputs is not null
                ? record : null;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException)
        {
            return null;
        }
    }

    private async Task writeRecordAsync(ExportRecord record, CancellationToken cancellationToken)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(recordPath)!);
        string temporaryPath = recordPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            await File.WriteAllTextAsync(temporaryPath, JsonSerializer.Serialize(record, jsonOptions), cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            File.Move(temporaryPath, recordPath, true);
        }
        finally
        {
            File.Delete(temporaryPath);
        }
    }

    private static bool sameInputs(ExportSnapshot first, ExportSnapshot second) =>
        first.GeneratorIdentity == second.GeneratorIdentity
        && first.Participants.SequenceEqual(second.Participants, StringComparer.Ordinal)
        && equalHashes(first.Inputs, second.Inputs);

    private static bool equalHashes(SortedDictionary<string, string> first, SortedDictionary<string, string> second) =>
        first.Count == second.Count && first.All(pair => second.TryGetValue(pair.Key, out string? hash) && hash == pair.Value);

    private bool setResult(bool current, string detail)
    {
        Detail = detail;
        return current;
    }

    private static bool isExportException(Exception exception) => exception is IOException or InvalidDataException or UnauthorizedAccessException
        or JsonException or InvalidOperationException or ArgumentException or KeyNotFoundException;

    private void onFileChanged(object sender, FileSystemEventArgs args)
    {
        if (isRecordPath(args.FullPath))
            RefreshAvailability();
    }

    private void onFileRenamed(object sender, RenamedEventArgs args)
    {
        if (isRecordPath(args.FullPath) || isRecordPath(args.OldFullPath))
            RefreshAvailability();
    }

    private bool isRecordPath(string path) => path.Equals(recordPath, StringComparison.OrdinalIgnoreCase)
        || path.Equals(Path.GetDirectoryName(recordPath), StringComparison.OrdinalIgnoreCase);

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        watcher.Dispose();
        lifetime.Cancel();
    }

    private sealed record ExportRecord(int FormatVersion, bool Complete, ExportSnapshot Snapshot);

    private sealed record ExportSnapshot(
        string GeneratorIdentity,
        string[] Participants,
        SortedDictionary<string, string> Inputs,
        SortedDictionary<string, string> Outputs);
}
