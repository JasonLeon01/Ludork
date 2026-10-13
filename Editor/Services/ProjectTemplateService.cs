using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class ProjectTemplateService
{
    private static readonly HttpClient downloadClient = new() { Timeout = Timeout.InfiniteTimeSpan };
    private readonly HttpClient client;
    private readonly string cacheDirectory;
    private readonly bool development;
    private readonly string baseVersion;
    private readonly string gameVersion;

    public ProjectTemplateService() : this(downloadClient, EditorPaths.TemplateCacheDirectory,
        EditorVersionService.IsDevelopment, EditorVersionService.BaseVersion, EditorVersionService.GameVersion)
    {
    }

    internal ProjectTemplateService(HttpClient client, string cacheDirectory, bool development,
        string baseVersion, string gameVersion)
    {
        this.client = client;
        this.cacheDirectory = Path.GetFullPath(cacheDirectory);
        this.development = development;
        this.baseVersion = baseVersion;
        this.gameVersion = gameVersion;
    }

    public Task<string> CreateAsync(string targetPath, bool standalone, bool ffmpeg,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        return Task.Run(() => createAsync(Path.GetFullPath(targetPath), standalone, ffmpeg,
            new ThrottledProgress(progress), cancellationToken), cancellationToken);
    }

    private async Task<string> createAsync(string targetPath, bool standalone, bool ffmpeg,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        if (Path.Exists(targetPath))
            throw new IOException(LocaleService.Get("PROJECT_EXISTS"));
        string parentPath = Path.GetDirectoryName(targetPath)!;
        string stagingPath = Path.Combine(parentPath, $".{Path.GetFileName(targetPath)}.{Guid.NewGuid():N}.creating");
        Directory.CreateDirectory(stagingPath);
        try
        {
            if (development)
            {
                string templateName = GetLocalTemplateName(standalone, ffmpeg);
                string sourcePath = EditorRuntimePaths.FindDirectory(Path.Combine("Templates", templateName))
                    ?? throw new DirectoryNotFoundException(LocaleService.Get("PROJECT_TEMPLATE_MISSING"));
                await ProjectTemplateArchive.CopyDirectoryAsync(sourcePath, stagingPath, progress, cancellationToken);
            }
            else
            {
                await extractReleaseAsync(stagingPath, standalone, ffmpeg, progress, cancellationToken);
            }
            JsonObject config = new()
            {
                ["version"] = gameVersion,
                ["Cpp"] = !standalone,
                ["lastFileExplorerPath"] = "Assets",
                ["packaging"] = new JsonObject { ["version"] = "1.0.0", ["dev"] = false },
            };
            if (ffmpeg)
                config["ffmpeg"] = true;
            await File.WriteAllTextAsync(Path.Combine(stagingPath, "Main.proj"),
                config.ToJsonString(new() { WriteIndented = true }), cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            Directory.Move(stagingPath, targetPath);
            return Path.Combine(targetPath, "Main.proj");
        }
        finally
        {
            if (Directory.Exists(stagingPath))
                Directory.Delete(stagingPath, true);
        }
    }

    public static string GetLocalTemplateName(bool standalone, bool ffmpeg)
    {
        string name = standalone ? "Standalone" : "Cpp";
        return ffmpeg ? name + "-ffmpeg" : name;
    }

    private async Task extractReleaseAsync(string stagingPath, bool standalone, bool ffmpeg,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        string name = standalone ? "game-standalone" : "game-source";
        if (ffmpeg)
            name += "-ffmpeg";
        string archiveName = name + (OperatingSystem.IsWindows() ? ".zip" : ".tar.gz");
        string archivePath = Path.Combine(cacheDirectory, archiveName);
        if (File.Exists(archivePath))
        {
            try
            {
                await extractAndValidateAsync(archivePath, stagingPath, standalone, ffmpeg,
                    progress, cancellationToken);
                return;
            }
            catch (InvalidDataException)
            {
                File.Delete(archivePath);
                Directory.Delete(stagingPath, true);
                Directory.CreateDirectory(stagingPath);
            }
        }
        Directory.CreateDirectory(cacheDirectory);
        string downloadPath = Path.Combine(cacheDirectory, $".{archiveName}.{Guid.NewGuid():N}.download");
        try
        {
            await downloadAsync(archiveName, downloadPath, progress, cancellationToken);
            await extractAndValidateAsync(downloadPath, stagingPath, standalone, ffmpeg,
                progress, cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            File.Move(downloadPath, archivePath, true);
        }
        finally
        {
            File.Delete(downloadPath);
        }
    }

    private async Task downloadAsync(string archiveName, string downloadPath,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        progress.Report(new(ProjectTemplateStage.Downloading, 0, null));
        Uri uri = new($"https://github.com/JasonLeon01/Ludork/releases/download/v{baseVersion}/{archiveName}");
        using HttpRequestMessage request = new(HttpMethod.Get, uri);
        request.Headers.UserAgent.ParseAdd($"Ludork/{baseVersion}");
        using HttpResponseMessage response = await client.SendAsync(request,
            HttpCompletionOption.ResponseHeadersRead, cancellationToken);
        if (response.StatusCode == HttpStatusCode.NotFound)
            throw new IOException(string.Format(LocaleService.Get("NEW_PROJECT_TEMPLATE_UNAVAILABLE"),
                baseVersion, archiveName));
        response.EnsureSuccessStatusCode();
        long? total = response.Content.Headers.ContentLength;
        progress.Report(new(ProjectTemplateStage.Downloading, 0, total));
        await using Stream source = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using FileStream target = new(downloadPath, FileMode.CreateNew, FileAccess.Write,
            FileShare.None, 65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        byte[] buffer = new byte[65536];
        long completed = 0;
        int count;
        while ((count = await source.ReadAsync(buffer, cancellationToken)) != 0)
        {
            await target.WriteAsync(buffer.AsMemory(0, count), cancellationToken);
            completed += count;
            progress.Report(new(ProjectTemplateStage.Downloading, completed, total));
        }
        if (total.HasValue && completed != total.Value)
            throw new InvalidDataException(LocaleService.Get("NEW_PROJECT_DOWNLOAD_INCOMPLETE"));
        progress.Report(new(ProjectTemplateStage.Downloading, completed, completed));
    }

    private async Task extractAndValidateAsync(string archivePath, string stagingPath,
        bool standalone, bool ffmpeg, IProgress<ProjectTemplateProgress> progress,
        CancellationToken cancellationToken)
    {
        await ProjectTemplateArchive.ExtractAsync(archivePath, stagingPath,
            OperatingSystem.IsWindows(), progress, cancellationToken);
        string projectFile = Path.Combine(stagingPath, "Main.proj");
        if (!File.Exists(projectFile))
            throw new InvalidDataException(LocaleService.Get("NEW_PROJECT_TEMPLATE_INVALID"));
        try
        {
            await using FileStream stream = File.OpenRead(projectFile);
            using JsonDocument document = await JsonDocument.ParseAsync(stream, cancellationToken: cancellationToken);
            JsonElement config = document.RootElement;
            if (config.ValueKind != JsonValueKind.Object
                || !config.TryGetProperty("version", out JsonElement version)
                || version.ValueKind != JsonValueKind.String || version.GetString() != gameVersion
                || !config.TryGetProperty("Cpp", out JsonElement cpp)
                || cpp.ValueKind != (standalone ? JsonValueKind.False : JsonValueKind.True)
                || (config.TryGetProperty("ffmpeg", out JsonElement video)
                    ? video.ValueKind != (ffmpeg ? JsonValueKind.True : JsonValueKind.False)
                    : ffmpeg))
                throw new InvalidDataException(LocaleService.Get("NEW_PROJECT_TEMPLATE_INVALID"));
        }
        catch (JsonException exception)
        {
            throw new InvalidDataException(LocaleService.Get("NEW_PROJECT_TEMPLATE_INVALID"), exception);
        }
    }

    private sealed class ThrottledProgress(IProgress<ProjectTemplateProgress> progress) : IProgress<ProjectTemplateProgress>
    {
        private readonly Stopwatch timer = Stopwatch.StartNew();
        private ProjectTemplateProgress? previous;

        public void Report(ProjectTemplateProgress value)
        {
            if (previous == value)
                return;
            if (previous?.Stage == value.Stage && timer.ElapsedMilliseconds < 100
                && (!value.TotalBytes.HasValue || value.CompletedBytes != value.TotalBytes.Value))
                return;
            previous = value;
            timer.Restart();
            progress.Report(value);
        }
    }
}
