using Avalonia.Threading;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class UiPreviewRuntimeService : IDisposable, IAsyncDisposable
{
    private const string RegistryFileName = "UiPreview.registry.json";
    private readonly DispatcherTimer timer;
    private readonly SemaphoreSlim refreshLock = new(1, 1);
    private readonly object connectionSync = new();
    private readonly HashSet<PreviewHostConnection> connections = [];
    private Task connectionShutdown = Task.CompletedTask;
    private string? manifestHash;
    private string? sourceStamp;
    private string? recoveryAttemptStamp;
    private string? recoveryError;
    private bool sourceUnavailable = true;
    private volatile bool disposed;

    public UiPreviewRuntimeService(string projectPath)
    {
        ProjectPath = Path.GetFullPath(projectPath);
        timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        timer.Tick += onTimerTick;
        timer.Start();
        StatusMessage = LocaleService.Get("UI_PREVIEW_COMPILE_REQUIRED");
        _ = RefreshAsync();
    }

    public string ProjectPath { get; }
    public UiPreviewRuntimeSnapshot? Current { get; private set; }
    public bool IsReady => !disposed && Current is not null && !sourceUnavailable;
    public string StatusMessage { get; private set; } = string.Empty;
    public event EventHandler? Changed;

    public async Task RefreshAsync(CancellationToken cancellationToken = default)
    {
        if (disposed)
            return;
        await refreshLock.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            if (disposed)
                return;
            try
            {
                SnapshotReadResult next =
                    await Task.Run(readCurrent, cancellationToken).ConfigureAwait(false);
                if (next.MetadataMissing)
                {
                    if (!sourceUnavailable || recoveryError is null)
                        applyState(next, LocaleService.Get("UI_PREVIEW_COMPILE_REQUIRED"));
                    if (await Task.Run(recoverStandaloneRegistry, cancellationToken).ConfigureAwait(false))
                        next = await Task.Run(readCurrent, cancellationToken).ConfigureAwait(false);
                }
                applyState(next, next.Building ? LocaleService.Get("UI_PREVIEW_BUILD_REQUIRED")
                    : next.Snapshot is null ? LocaleService.Get("UI_PREVIEW_COMPILE_REQUIRED") : string.Empty);
            }
            catch (Exception exception) when (IsSnapshotException(exception))
            {
                applyState(null, LocaleService.Get("UI_PREVIEW_SNAPSHOT_INVALID") + Environment.NewLine + exception.Message);
            }
        }
        finally
        {
            refreshLock.Release();
        }
    }

    private void applyState(SnapshotReadResult? next, string message)
    {
        if (disposed)
            return;
        UiPreviewRuntimeSnapshot? previous = Current;
        string previousMessage = StatusMessage;
        bool previouslyUnavailable = sourceUnavailable;
        if (next?.Building == true)
        {
            recoveryAttemptStamp = null;
            recoveryError = null;
        }
        if (next?.Snapshot is not null)
        {
            manifestHash = next.Hash;
            sourceStamp = next.Stamp;
            Current = next.Snapshot;
            recoveryAttemptStamp = null;
            recoveryError = null;
        }
        sourceUnavailable = next?.Snapshot is null;
        StatusMessage = message;
        if (previous?.BuildId != Current?.BuildId
            || previous?.RegistryHash != Current?.RegistryHash
            || previous?.HostPath != Current?.HostPath
            || previousMessage != StatusMessage
            || previouslyUnavailable != sourceUnavailable)
        {
            Changed?.Invoke(this, EventArgs.Empty);
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        timer.Stop();
        timer.Tick -= onTimerTick;
        PreviewHostConnection[] active;
        lock (connectionSync)
            active = connections.ToArray();
        foreach (PreviewHostConnection connection in active)
            connection.Dispose();
        connectionShutdown = Task.WhenAll(active.Select(connection => connection.Shutdown));
    }

    public async ValueTask DisposeAsync()
    {
        Dispose();
        await connectionShutdown.ConfigureAwait(false);
        await refreshLock.WaitAsync().ConfigureAwait(false);
        refreshLock.Release();
    }

    internal void RegisterConnection(PreviewHostConnection connection)
    {
        lock (connectionSync)
        {
            ObjectDisposedException.ThrowIf(disposed, this);
            connections.Add(connection);
        }
    }

    internal void UnregisterConnection(PreviewHostConnection connection)
    {
        lock (connectionSync)
            connections.Remove(connection);
    }

    private async void onTimerTick(object? sender, EventArgs args)
    {
        if (refreshLock.CurrentCount != 0)
            await RefreshAsync();
    }

    private SnapshotReadResult readCurrent()
    {
        string metadataRoot = Path.Combine(ProjectPath, "Temp");
        string manifestPath = Path.Combine(metadataRoot, "UiPreview.json");
        string registryPath = Path.Combine(metadataRoot, RegistryFileName);
        requireNotLink(metadataRoot);
        string buildingPath = Path.Combine(metadataRoot, "UiPreview.building");
        requireNotLink(buildingPath);
        if (Path.Exists(buildingPath))
            return new SnapshotReadResult(null, null, null, Building: true);
        requireNotLink(manifestPath);
        requireNotLink(registryPath);
        if (!File.Exists(manifestPath) || !File.Exists(registryPath))
            return new SnapshotReadResult(null, null, null, MetadataMissing: true);
        using FileStream manifest = openSharedRead(manifestPath);
        using MemoryStream buffer = new();
        manifest.CopyTo(buffer);
        byte[] manifestBytes = buffer.ToArray();
        string nextHash = hash(manifestBytes);
        JsonObject manifestData = readObject(manifestBytes);
        requireFields(manifestData, ["formatVersion", "buildId", "platform", "architecture", "configuration",
            "files", "registryHash", "adapterFingerprint", "runtimeDirectory"]);
        require(manifestData["formatVersion"]?.GetValue<int>() == 3, "Unsupported UI preview manifest version.");
        string root = resolveRuntimeDirectory(readString(manifestData, "runtimeDirectory"));
        if (Current is not null && nextHash == manifestHash)
        {
            string stamp = createSourceStamp(root, Current.Files, registryPath);
            if (stamp == sourceStamp)
                return Path.Exists(buildingPath)
                    ? new SnapshotReadResult(null, null, null, Building: true)
                    : new SnapshotReadResult(nextHash, stamp, Current);
        }
        (UiPreviewRuntimeSnapshot snapshot, string verifiedStamp) = readSnapshot(root, registryPath, manifestData);
        if (Path.Exists(buildingPath))
            return new SnapshotReadResult(null, null, null, Building: true);
        return new SnapshotReadResult(nextHash, verifiedStamp, snapshot);
    }

    private static (UiPreviewRuntimeSnapshot Snapshot, string Stamp) readSnapshot(
        string root,
        string registryPath,
        JsonObject manifest)
    {
        string buildId = readHash(manifest, "buildId");
        string registryHash = readHash(manifest, "registryHash");
        string fingerprint = readHash(manifest, "adapterFingerprint");
        string platform = OperatingSystem.IsWindows() ? "win32"
            : OperatingSystem.IsMacOS() ? "darwin" : OperatingSystem.IsLinux() ? "linux" : string.Empty;
        string architecture = RuntimeInformation.ProcessArchitecture switch
        {
            Architecture.X64 => "x64",
            Architecture.Arm64 => "arm64",
            Architecture.X86 => "x86",
            Architecture.Arm => "arm",
            _ => string.Empty,
        };
        require(platform.Length != 0 && readString(manifest, "platform") == platform,
            "UI preview runtime platform does not match this editor.");
        require(architecture.Length != 0 && readString(manifest, "architecture") == architecture,
            "UI preview runtime architecture does not match this editor.");
        require(readString(manifest, "configuration") is "Debug" or "Release", "Invalid UI preview configuration.");
        string hostName = OperatingSystem.IsWindows() ? "UiPreviewHost.exe" : "UiPreviewHost";
        IReadOnlyList<string> files = readFiles(manifest);
        require(files.Contains(hostName, StringComparer.Ordinal), "UI preview manifest must list its executable.");
        string verifiedStamp = createSourceStamp(root, files, registryPath);
        require(runtimeDigest(root, files, registryPath) == buildId, "UI preview runtime files do not match the build ID.");
        string hostPath = Path.Combine(root, hostName);
        requireNotLink(hostPath);
        require(File.Exists(hostPath), "The project UI preview executable is missing.");
        requireNotLink(registryPath);
        using FileStream registryStream = openSharedRead(registryPath);
        using MemoryStream registryBuffer = new();
        registryStream.CopyTo(registryBuffer);
        byte[] registryBytes = registryBuffer.ToArray();
        require(hash(registryBytes) == registryHash, "UI registry hash does not match UiPreview.json.");
        JsonObject registry = readObject(registryBytes);
        requireFields(registry, ["formatVersion", "adapterFingerprint", "controls"]);
        require(registry["formatVersion"]?.GetValue<int>() == 1, "Unsupported UI registry version.");
        require(readHash(registry, "adapterFingerprint") == fingerprint, "UI registry fingerprint does not match UiPreview.json.");
        JsonArray controls = registry["controls"] as JsonArray
            ?? throw new InvalidDataException("UI registry controls must be an array.");
        List<UiControlDescriptor> descriptors = [];
        HashSet<string> controlIds = new(StringComparer.Ordinal);
        foreach (JsonNode? value in controls)
        {
            JsonObject control = value as JsonObject
                ?? throw new InvalidDataException("UI registry control must be an object.");
            requireFields(control, ["controlId", "source", "adapter", "displayName", "category",
                "childPolicy", "slotType", "textKind", "properties"]);
            string controlId = readString(control, "controlId");
            require(UiControlRegistryService.IsCanonicalControlId(controlId), "Invalid UI control ID.");
            require(controlIds.Add(controlId), "Duplicate UI control: " + controlId);
            require(readString(control, "source") == "system", "Compiled UI controls must have system source.");
            require(readString(control, "adapter") == controlId, "UI control must identify its canonical native adapter.");
            string childPolicy = readString(control, "childPolicy");
            string? slotType = readNullableString(control, "slotType");
            string? textKind = readNullableString(control, "textKind");
            require(childPolicy is "none" or "single" or "multiple", "Invalid UI child policy.");
            require(slotType is null or "canvas" or "list", "Invalid UI slot type.");
            require((childPolicy == "none") == (slotType is null), "Inconsistent UI child policy and slot type.");
            require(textKind is null or "plain" or "rich", "Invalid UI text kind.");
            JsonArray properties = control["properties"] as JsonArray
                ?? throw new InvalidDataException("UI control properties must be an array.");
            List<UiControlPropertyDescriptor> propertyDescriptors = [];
            HashSet<string> propertyIds = new(StringComparer.Ordinal);
            foreach (JsonNode? propertyValue in properties)
            {
                JsonObject property = propertyValue as JsonObject
                    ?? throw new InvalidDataException("UI control property must be an object.");
                requireFields(property, ["id", "displayName", "type", "required", "default", "editorOnly", "adapterProperty"]);
                string id = readString(property, "id");
                require(propertyIds.Add(id), "Duplicate UI property: " + id);
                string type = readString(property, "type");
                bool editorOnly = readBoolean(property, "editorOnly");
                bool adapterProperty = readBoolean(property, "adapterProperty");
                require(!editorOnly || !adapterProperty, "Editor-only UI property cannot belong to the adapter.");
                require(UiAssetValidationService.IsPropertyValueValid(property["default"], type),
                    "Invalid UI property default: " + controlId + "." + id);
                propertyDescriptors.Add(new UiControlPropertyDescriptor(
                    id,
                    readString(property, "displayName"),
                    type,
                    readBoolean(property, "required"),
                    property["default"]?.DeepClone(),
                    editorOnly,
                    adapterProperty));
            }
            require(textKind is null || propertyIds.Contains("textConfig"), "Text control has no textConfig property.");
            descriptors.Add(new UiControlDescriptor(
                controlId, "system", readString(control, "displayName"), readString(control, "category"),
                readString(control, "adapter"), childPolicy, slotType, propertyDescriptors,
                TextKind: textKind));
        }
        require(descriptors.Count != 0, "UI registry has no controls.");
        require(UiControlRegistryService.CreateAdapterFingerprint(descriptors) == fingerprint,
            "UI registry descriptors do not match the adapter fingerprint.");
        require(createSourceStamp(root, files, registryPath) == verifiedStamp, "UI preview files changed during validation.");
        return (new UiPreviewRuntimeSnapshot(buildId, hostPath, registryHash, fingerprint, descriptors, files), verifiedStamp);
    }

    private string resolveRuntimeDirectory(string relativePath)
    {
        string[] parts = relativePath.Split('/');
        require(!Path.IsPathRooted(relativePath) && !relativePath.Contains('\\')
            && !string.Equals(parts[0], "Temp", StringComparison.OrdinalIgnoreCase)
            && parts.All(isSafeFileName), "UI preview runtime directory must be a safe project-relative path.");
        string path = ProjectPath;
        foreach (string part in parts)
        {
            path = Path.Combine(path, part);
            requireNotLink(path);
        }
        return path;
    }

    private bool recoverStandaloneRegistry()
    {
        if (disposed)
            return false;
        string metadataRoot = Path.Combine(ProjectPath, "Temp");
        string buildingPath = Path.Combine(metadataRoot, "UiPreview.building");
        if (Path.Exists(buildingPath))
            return false;
        string configPath = Path.Combine(ProjectPath, "Main.proj");
        if (!File.Exists(configPath))
            return false;
        JsonObject config = readObject(File.ReadAllBytes(configPath));
        if (config["Cpp"] is not JsonValue cppValue || !cppValue.TryGetValue(out bool cpp) || cpp)
            return false;
        string binaryRoot = Path.Combine(ProjectPath, "Binaries");
        requireNotLink(binaryRoot);
        string executable = OperatingSystem.IsWindows() ? "ScriptTools.exe" : "ScriptTools";
        string? toolPath = EditorRuntimePaths.FindFile("tools", executable)
            ?? EditorRuntimePaths.FindFile(".tools", "ScriptTools", executable);
        string manifestPath = Path.Combine(metadataRoot, "UiPreview.json");
        string registryPath = Path.Combine(metadataRoot, RegistryFileName);
        List<string> paths = [configPath, manifestPath, registryPath];
        if (toolPath is not null)
            paths.Add(toolPath);
        if (Directory.Exists(binaryRoot))
            paths.AddRange(Directory.EnumerateFiles(binaryRoot));
        StringBuilder stamp = new();
        foreach (string path in paths.OrderBy(path => path, Utf8NameComparer.Instance))
        {
            FileInfo file = new(path);
            stamp.Append(path).Append('\0').Append(file.Exists ? file.Length : -1)
                .Append(':').Append(file.LastWriteTimeUtc.Ticks).Append(':').Append(file.LinkTarget).Append('\0');
        }
        string nextStamp = stamp.ToString();
        if (recoveryAttemptStamp == nextStamp)
        {
            require(recoveryError is null, recoveryError ?? string.Empty);
            return false;
        }
        recoveryAttemptStamp = nextStamp;
        try
        {
            require(toolPath is not null, "ScriptTools was not found in the editor installation.");
            if (disposed || Path.Exists(buildingPath))
                return false;
            ProcessStartInfo startInfo = new()
            {
                FileName = toolPath!,
                WorkingDirectory = ProjectPath,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8,
            };
            startInfo.ArgumentList.Add("ui-preview");
            startInfo.ArgumentList.Add("ensure");
            startInfo.ArgumentList.Add(ProjectPath);
            startInfo.Environment["PYTHONUTF8"] = "1";
            startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
            using Process process = Process.Start(startInfo)
                ?? throw new IOException("ScriptTools could not be started.");
            Task<string> output = process.StandardOutput.ReadToEndAsync();
            Task<string> error = process.StandardError.ReadToEndAsync();
            process.WaitForExit();
            string outputText = output.GetAwaiter().GetResult().Trim();
            string errorText = error.GetAwaiter().GetResult().Trim();
            require(process.ExitCode == 0, errorText.Length != 0 ? errorText
                : outputText.Length != 0 ? outputText : "Standalone UI preview recovery failed.");
            require(Path.Exists(buildingPath) || File.Exists(manifestPath) && File.Exists(registryPath),
                "Standalone UI preview recovery did not produce its metadata files.");
            recoveryError = null;
            return true;
        }
        catch (Exception exception) when (IsSnapshotException(exception))
        {
            recoveryError = exception.Message;
            throw;
        }
    }

    private static JsonObject readObject(byte[] bytes)
    {
        using JsonDocument document = JsonDocument.Parse(new UTF8Encoding(false, true).GetString(bytes));
        validateUniqueFields(document.RootElement);
        return JsonNode.Parse(document.RootElement.GetRawText()) as JsonObject
            ?? throw new InvalidDataException("UI preview metadata must be a JSON object.");
    }

    private static void validateUniqueFields(JsonElement value)
    {
        if (value.ValueKind == JsonValueKind.Object)
        {
            HashSet<string> names = new(StringComparer.Ordinal);
            foreach (JsonProperty property in value.EnumerateObject())
            {
                require(names.Add(property.Name), "Duplicate UI preview JSON field: " + property.Name);
                validateUniqueFields(property.Value);
            }
        }
        else if (value.ValueKind == JsonValueKind.Array)
        {
            foreach (JsonElement item in value.EnumerateArray())
                validateUniqueFields(item);
        }
    }

    private static void requireFields(JsonObject value, string[] fields)
    {
        require(value.Count == fields.Length && fields.All(value.ContainsKey), "Unexpected UI preview metadata fields.");
    }

    private static void requireNotLink(string path)
    {
        FileSystemInfo entry = Directory.Exists(path) ? new DirectoryInfo(path) : new FileInfo(path);
        require(entry.LinkTarget is null && (!entry.Exists || (entry.Attributes & FileAttributes.ReparsePoint) == 0),
            "UI preview metadata and directories must not be links: " + path);
    }

    private static string readString(JsonObject value, string key)
    {
        string? result = value[key]?.GetValue<string>();
        require(!string.IsNullOrWhiteSpace(result) && result == result.Trim() && !result.Contains('\0'),
            "UI preview metadata requires " + key + " without surrounding whitespace or NUL.");
        return result!;
    }

    private static string? readNullableString(JsonObject value, string key)
    {
        require(value.ContainsKey(key), "UI preview metadata requires " + key + ".");
        return value[key] is null ? null : readString(value, key);
    }

    private static bool readBoolean(JsonObject value, string key)
    {
        return value[key]?.GetValue<bool>()
            ?? throw new InvalidDataException("UI preview metadata requires " + key + ".");
    }

    private static string readHash(JsonObject value, string key)
    {
        string result = readString(value, key);
        require(result.Length == 64 && result.All(character => character is >= '0' and <= '9' or >= 'a' and <= 'f'),
            "Invalid UI preview " + key + ".");
        return result;
    }

    private static string hash(byte[] value) => Convert.ToHexString(SHA256.HashData(value)).ToLowerInvariant();

    private static bool IsSnapshotException(Exception exception)
    {
        return exception is IOException or InvalidDataException or UnauthorizedAccessException
            or JsonException or InvalidOperationException or FormatException or OverflowException
            or DecoderFallbackException or Win32Exception;
    }

    private static IReadOnlyList<string> readFiles(JsonObject manifest)
    {
        JsonArray values = manifest["files"] as JsonArray
            ?? throw new InvalidDataException("UI preview files must be an array.");
        List<string> files = [];
        HashSet<string> seen = new(StringComparer.OrdinalIgnoreCase);
        foreach (JsonNode? value in values)
        {
            string? name = value?.GetValue<string>();
            require(name is not null && isSafeFileName(name), "UI preview files must use safe flat file names.");
            require(seen.Add(name!), "UI preview file names must be unique.");
            files.Add(name!);
        }
        require(files.Count != 0, "UI preview manifest has no files.");
        return files;
    }

    private static bool isSafeFileName(string name)
        => isSafePathSegment(name)
            && !string.Equals(name, "UiPreview.json", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(name, RegistryFileName, StringComparison.OrdinalIgnoreCase);

    private static bool isSafePathSegment(string name)
    {
        string stem = name.Split('.')[0].ToUpperInvariant();
        if (stem is "CON" or "PRN" or "AUX" or "NUL" or "CLOCK$"
            || stem.Length == 4 && (stem.StartsWith("COM", StringComparison.Ordinal)
                || stem.StartsWith("LPT", StringComparison.Ordinal)) && "123456789¹²³".Contains(stem[3]))
        {
            return false;
        }
        return name.Length != 0 && name == name.Trim() && name is not ("." or "..")
            && !name.EndsWith(".", StringComparison.Ordinal)
            && name.IndexOfAny(['/', '\\', ':', '\0', '<', '>', '"', '|', '?', '*']) < 0
            && !name.Any(char.IsControl);
    }

    private static FileStream openSharedRead(string path)
        => new(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);

    private static string createSourceStamp(string directory, IReadOnlyList<string> files, string registryPath)
    {
        StringBuilder stamp = new();
        foreach (string name in files.Append(RegistryFileName).OrderBy(name => name, Utf8NameComparer.Instance))
        {
            FileInfo file = new(name == RegistryFileName ? registryPath : Path.Combine(directory, name));
            require(file.Exists, "UI preview file is missing: " + name);
            stamp.Append(name).Append('\0').Append(file.Length).Append(':')
                .Append(file.LastWriteTimeUtc.Ticks).Append(':').Append(file.LinkTarget).Append('\0');
        }
        return stamp.ToString();
    }

    private static string readRelativeLink(FileInfo file, string directory, IReadOnlyList<string> files)
    {
        string target = file.LinkTarget ?? throw new InvalidDataException("UI preview link target is missing.");
        string normalized = target.Replace('\\', '/');
        require(!Path.IsPathRooted(target) && !normalized.Contains(':')
            && !normalized.Split('/').Any(part => part == ".."), "UI preview runtime links must stay relative to their directory.");
        string fullTarget = Path.GetFullPath(Path.Combine(directory, target));
        FileSystemInfo? resolved = file.ResolveLinkTarget(true);
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        require(string.Equals(Path.GetDirectoryName(fullTarget), directory, comparison)
            && files.Contains(Path.GetFileName(fullTarget), StringComparer.Ordinal)
            && resolved is not null && resolved.Exists
            && string.Equals(Path.GetDirectoryName(resolved.FullName), directory, comparison)
            && files.Contains(resolved.Name, StringComparer.Ordinal),
            "UI preview runtime link escapes its listed files or is broken.");
        return normalized;
    }

    private static string runtimeDigest(string directory, IReadOnlyList<string> files, string registryPath)
    {
        requireNotLink(registryPath);
        using IncrementalHash digest = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (string name in files.Append(RegistryFileName).OrderBy(name => name, Utf8NameComparer.Instance))
        {
            FileInfo file = new(name == RegistryFileName ? registryPath : Path.Combine(directory, name));
            require((file.Attributes & FileAttributes.Directory) == 0,
                "UI preview runtime must contain only files.");
            digest.AppendData(Encoding.UTF8.GetBytes(name + "\0"));
            string? target = file.LinkTarget;
            if (target is not null)
            {
                digest.AppendData(Encoding.UTF8.GetBytes("L\0" + readRelativeLink(file, directory, files) + "\0"));
            }
            else
            {
                require((file.Attributes & FileAttributes.ReparsePoint) == 0,
                    "Unsupported UI preview runtime file link.");
                using FileStream stream = openSharedRead(file.FullName);
                string fileHash = Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
                digest.AppendData(Encoding.ASCII.GetBytes("F\0" + fileHash + "\0"));
            }
        }
        return Convert.ToHexString(digest.GetHashAndReset()).ToLowerInvariant();
    }

    private sealed class Utf8NameComparer : IComparer<string>
    {
        public static Utf8NameComparer Instance { get; } = new();

        public int Compare(string? left, string? right)
        {
            return Encoding.UTF8.GetBytes(left ?? string.Empty).AsSpan()
                .SequenceCompareTo(Encoding.UTF8.GetBytes(right ?? string.Empty));
        }
    }

    private sealed record SnapshotReadResult(
        string? Hash,
        string? Stamp,
        UiPreviewRuntimeSnapshot? Snapshot,
        bool Building = false,
        bool MetadataMissing = false);

    private static void require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidDataException(message);
    }
}
