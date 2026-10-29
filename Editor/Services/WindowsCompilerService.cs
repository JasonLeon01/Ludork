using SharpCompress.Archives.SevenZip;
using SharpCompress.Archives;
using SharpCompress.Readers;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public static class WindowsCompilerService
{
    private const string ArchiveUrl = "https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-19.1.1-12.0.0-ucrt-r2/winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r2.7z";
    private const string ArchiveSha256 = "e8751bcad522ee112fbc197e6224c67811df6d039cf400269135eff0dad8cf0c";
    private static readonly HttpClient client = new() { Timeout = Timeout.InfiniteTimeSpan };
    private static readonly SemaphoreSlim installationLock = new(1, 1);
    private static string installationDirectory => Path.Combine(EditorPaths.UserDataDirectory,
        "Toolchains", "WinLibs-14.2.0-ucrt-r2");
    public static string ManagedCompilerPath => Path.Combine(installationDirectory, "mingw64", "bin", "g++.exe");

    public static async Task<IReadOnlyList<WindowsCompiler>> ListAsync(string? projectPath = null,
        CancellationToken cancellationToken = default)
    {
        if (!OperatingSystem.IsWindows())
            return [];
        List<string> arguments = ["list"];
        if (projectPath is not null)
            arguments.AddRange(["--project", projectPath]);
        JsonElement result = await executeAsync(arguments, cancellationToken);
        return result.GetProperty("compilers").EnumerateArray().Select(item => new WindowsCompiler(
            item.GetProperty("name").GetString()!, normalize(item.GetProperty("compiler").GetString()),
            item.GetProperty("source").GetString()!,
            item.TryGetProperty("version", out JsonElement version) ? version.GetString() ?? string.Empty : string.Empty)).ToArray();
    }

    public static async Task ValidateAsync(string? compiler, CancellationToken cancellationToken = default)
        => await executeAsync(compilerArguments("validate", null, compiler), cancellationToken);

    public static async Task<WindowsCompilerStatus> StatusAsync(string projectPath, string? compiler,
        CancellationToken cancellationToken = default)
    {
        JsonElement result = await executeAsync(compilerArguments("status", projectPath, compiler), cancellationToken);
        return new(result.GetProperty("needsCleanup").GetBoolean(),
            normalize(result.GetProperty("currentCompiler").GetString()),
            normalize(result.GetProperty("targetCompiler").GetString()));
    }

    public static async Task SwitchAsync(string projectPath, string? compiler, string? expectedCompiler)
    {
        List<string> arguments = compilerArguments("switch", projectPath, compiler);
        if (expectedCompiler is not null)
            arguments.AddRange(["--expected-compiler", expectedCompiler]);
        JsonElement result = await executeAsync(arguments, CancellationToken.None);
        if (result.GetProperty("needsCleanup").GetBoolean())
            throw new IOException("Native compiler cleanup did not finish.");
    }

    public static bool SameCompiler(string? first, string? second) =>
        string.Equals(normalize(first), normalize(second), StringComparison.OrdinalIgnoreCase);

    public static async Task InstallAsync(IProgress<ProjectTemplateProgress> progress,
        CancellationToken cancellationToken = default)
    {
        await installationLock.WaitAsync(cancellationToken);
        try
        {
            if (File.Exists(ManagedCompilerPath))
            {
                IReadOnlyList<WindowsCompiler> installed = await ListAsync(null, cancellationToken);
                if (installed.Any(compiler => SameCompiler(compiler.Compiler, ManagedCompilerPath)))
                    return;
            }
            string parent = Path.GetDirectoryName(installationDirectory)!;
            Directory.CreateDirectory(parent);
            string staging = Path.Combine(parent, ".winlibs-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(staging);
            try
            {
                string archivePath = Path.Combine(staging, "download.7z");
                using HttpRequestMessage request = new(HttpMethod.Get, ArchiveUrl);
                request.Headers.UserAgent.ParseAdd("Ludork/" + EditorVersionService.BaseVersion);
                using HttpResponseMessage response = await client.SendAsync(request,
                    HttpCompletionOption.ResponseHeadersRead, cancellationToken);
                response.EnsureSuccessStatusCode();
                await DownloadProgressService.CopyAsync(response, archivePath, progress, cancellationToken);
                await using (FileStream archiveStream = File.OpenRead(archivePath))
                {
                    byte[] digest = await SHA256.HashDataAsync(archiveStream, cancellationToken);
                    if (!Convert.ToHexStringLower(digest).Equals(ArchiveSha256, StringComparison.Ordinal))
                        throw new InvalidDataException("WinLibs SHA-256 verification failed.");
                }
                string extracted = Path.Combine(staging, "extracted");
                try
                {
                    await Task.Run(() => extractAsync(archivePath, extracted, progress, cancellationToken), cancellationToken);
                }
                catch (SharpCompress.Common.SharpCompressException exception)
                {
                    throw new InvalidDataException(exception.Message, exception);
                }
                await ValidateAsync(Path.Combine(extracted, "mingw64", "bin", "g++.exe"), cancellationToken);
                cancellationToken.ThrowIfCancellationRequested();
                string retired = Path.Combine(staging, "previous");
                if (Directory.Exists(installationDirectory))
                {
                    if ((File.GetAttributes(installationDirectory) & FileAttributes.ReparsePoint) != 0)
                        throw new IOException("The WinLibs installation directory must not be a link.");
                    Directory.Move(installationDirectory, retired);
                }
                try
                {
                    Directory.Move(extracted, installationDirectory);
                }
                catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                {
                    if (Directory.Exists(retired) && !Directory.Exists(installationDirectory))
                        Directory.Move(retired, installationDirectory);
                    throw;
                }
            }
            finally
            {
                if (Directory.Exists(staging))
                    Directory.Delete(staging, true);
            }
        }
        finally
        {
            installationLock.Release();
        }
    }

    private static async Task extractAsync(string archivePath, string target,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        Directory.CreateDirectory(target);
        string prefix = Path.GetFullPath(target) + Path.DirectorySeparatorChar;
        using IArchive archive = SevenZipArchive.OpenArchive(archivePath);
        long total = archive.Entries.Sum(entry => entry.Size);
        long completed = 0;
        Stopwatch throttle = Stopwatch.StartNew();
        progress.Report(new(ProjectTemplateStage.Extracting, completed, total));
        using IReader reader = archive.ExtractAllEntries();
        while (reader.MoveToNextEntry())
        {
            cancellationToken.ThrowIfCancellationRequested();
            string key = reader.Entry.Key ?? throw new InvalidDataException("Archive entry has no path.");
            string path = Path.GetFullPath(Path.Combine(target, key));
            if (!path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)
                || Path.IsPathRooted(key) || key.Contains(':') || reader.Entry.LinkTarget is not null)
                throw new InvalidDataException("Unsafe WinLibs archive entry: " + key);
            if (reader.Entry.IsDirectory)
                Directory.CreateDirectory(path);
            else
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                using Stream source = reader.OpenEntryStream();
                await using FileStream destination = new(path, FileMode.CreateNew, FileAccess.Write,
                    FileShare.None, 65536, FileOptions.Asynchronous);
                await source.CopyToAsync(destination, cancellationToken);
                completed += reader.Entry.Size;
                if (throttle.ElapsedMilliseconds >= 100 || completed == total)
                {
                    progress.Report(new(ProjectTemplateStage.Extracting, completed, total));
                    throttle.Restart();
                }
            }
        }
    }

    private static string? normalize(string? compiler) => string.IsNullOrEmpty(compiler) ? null : compiler;

    private static List<string> compilerArguments(string operation, string? projectPath, string? compiler)
    {
        List<string> arguments = [operation];
        if (projectPath is not null)
            arguments.Add(projectPath);
        if (compiler is not null)
            arguments.AddRange(["--compiler", compiler]);
        return arguments;
    }

    private static async Task<JsonElement> executeAsync(IEnumerable<string> arguments, CancellationToken token)
    {
        string tool = EditorRuntimePaths.FindScriptTools()
            ?? throw new FileNotFoundException("ScriptTools was not found in the editor installation.");
        ProcessStartInfo startInfo = new()
        {
            FileName = tool,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };
        startInfo.ArgumentList.Add("windows-toolchain");
        foreach (string argument in arguments)
            startInfo.ArgumentList.Add(argument);
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        token.ThrowIfCancellationRequested();
        using Process process = new() { StartInfo = startInfo };
        process.Start();
        Task<string> output = process.StandardOutput.ReadToEndAsync();
        Task<string> error = process.StandardError.ReadToEndAsync();
        using CancellationTokenRegistration registration = token.Register(() =>
        {
            try
            {
                if (!process.HasExited)
                    process.Kill(true);
            }
            catch (Exception exception) when (exception is Win32Exception or InvalidOperationException)
            {
            }
        });
        await process.WaitForExitAsync();
        string outputText = await output;
        string errorText = await error;
        token.ThrowIfCancellationRequested();
        if (process.ExitCode != 0)
            throw new IOException(errorText.Trim());
        using JsonDocument document = JsonDocument.Parse(outputText);
        return document.RootElement.Clone();
    }
}
