using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Services;

public enum ProjectPackFailure
{
    None,
    ProjectInvalid,
    AppNameUnchanged,
    PluginPreparationFailed,
    Cancelled,
    PlatformUnsupported,
    ScriptMissing,
    LaunchFailed,
    ToolchainUnavailable,
    DeviceUnavailable,
    SigningUnavailable,
    IOSProjectUnsupported,
    HarmonyToolchainUnavailable,
    HarmonyDeviceUnavailable,
    HarmonySigningUnavailable,
    HarmonyProjectUnsupported,
    AndroidToolchainUnavailable,
    AndroidSigningUnavailable,
    AndroidProjectUnsupported,
    PackFailed,
}

public enum ProjectPackPlatform
{
    Win32,
    MacOS,
    IOS,
    HarmonyOS,
    Android,
}

public enum HarmonyDeviceForm
{
    Mobile,
    TwoInOne,
}

public enum HarmonyGraphicsApi
{
    OpenGL,
    OpenGLES,
}

public sealed record AndroidSigningOptions(
    string KeystorePath,
    string KeyAlias,
    string KeystorePassword,
    string KeyPassword)
{
    public override string ToString() =>
        nameof(AndroidSigningOptions) + " { Redacted }";
}

public sealed record ProjectPackOptions(
    ProjectPackPlatform Platform,
    bool UseLuac,
    bool EncryptShaders,
    bool EncryptData)
{
    public bool ExportToIPhone { get; init; }
    public bool ExportToHarmonyDevice { get; init; }
    public HarmonyDeviceForm HarmonyDeviceForm { get; init; } =
        global::Ludork.Services.HarmonyDeviceForm.Mobile;
    public HarmonyGraphicsApi HarmonyGraphicsApi { get; init; } =
        global::Ludork.Services.HarmonyGraphicsApi.OpenGL;
    public AndroidSigningOptions? AndroidSigning { get; init; }
}

public sealed record ProjectPackResult(
    bool Success,
    ProjectPackFailure Failure,
    string Detail)
{
    public static ProjectPackResult Completed() => new(true, ProjectPackFailure.None, string.Empty);
    public static ProjectPackResult Failed(ProjectPackFailure failure, string detail) => new(false, failure, detail);
}

public sealed class ProjectPackService
{
    private const int AppNameUnchangedExitCode = 24;
    private const string CompileLuaDirectoriesEnvironment =
        "LUDORK_PACK_COMPILE_LUA_DIRECTORIES";
    private const string ExcludedFilesEnvironment =
        "LUDORK_PACK_EXCLUDED_FILES";
    private static readonly UTF8Encoding utf8 = new(false);
    private static readonly Regex defaultAppNamePattern = new(
        @"^[ \t]*local[ \t]+APP_NAME[ \t]*=[ \t]*[""']LudorkSample[""'][ \t]*(?:--[^\r\n]*)?\r?$",
        RegexOptions.Multiline | RegexOptions.CultureInvariant);
    private readonly string projectPath;
    private readonly ProjectOperationPipeline operationPipeline;

    public ProjectPackService(string projectPath, IEditorPluginRuntime? pluginRuntime = null)
    {
        if (!Path.IsPathFullyQualified(projectPath))
            throw new ArgumentException(nameof(projectPath));
        this.projectPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
        operationPipeline = new ProjectOperationPipeline(this.projectPath, pluginRuntime);
    }

    public event EventHandler<string>? OutputReceived;

    public static string? GetScriptName(ProjectPackPlatform platform)
    {
        if (platform == ProjectPackPlatform.Win32 && OperatingSystem.IsWindows())
            return "pack_project.bat";
        if (platform == ProjectPackPlatform.MacOS && OperatingSystem.IsMacOS())
            return "pack_project.sh";
        if (platform == ProjectPackPlatform.IOS && OperatingSystem.IsMacOS())
            return "pack_ios.sh";
        if (platform == ProjectPackPlatform.HarmonyOS && OperatingSystem.IsMacOS())
            return "pack_harmony.sh";
        if (platform == ProjectPackPlatform.Android && OperatingSystem.IsMacOS())
            return "pack_android.sh";
        return null;
    }

    public async Task<ProjectPackResult> PackAsync(
        ProjectPackOptions options,
        CancellationToken cancellationToken = default)
    {
        string projectFilePath = Path.Combine(projectPath, "Main.proj");
        if (!Directory.Exists(projectPath))
            return ProjectPackResult.Failed(ProjectPackFailure.ProjectInvalid, projectPath);
        if (!File.Exists(projectFilePath))
            return ProjectPackResult.Failed(ProjectPackFailure.ProjectInvalid, projectFilePath);

        ProjectPackResult? optionsFailure = validateOptions(options, projectFilePath);
        if (optionsFailure is not null)
            return optionsFailure;

        ProjectPackResult? appNameFailure = validateAppName();
        if (appNameFailure is not null)
            return appNameFailure;

        string? scriptName = GetScriptName(options.Platform);
        if (scriptName is null)
            return ProjectPackResult.Failed(ProjectPackFailure.PlatformUnsupported, options.Platform.ToString());

        string? scriptPath = EditorRuntimePaths.FindFile("tools", scriptName);
        if (scriptPath is null)
            return ProjectPackResult.Failed(ProjectPackFailure.ScriptMissing, "tools/" + scriptName);

        bool exportToIPhone = options.Platform == ProjectPackPlatform.IOS
            && options.ExportToIPhone;
        bool exportToHarmonyDevice = options.Platform == ProjectPackPlatform.HarmonyOS
            && options.ExportToHarmonyDevice;
        HarmonyDeviceForm? harmonyDeviceForm = options.Platform == ProjectPackPlatform.HarmonyOS
            ? options.HarmonyDeviceForm
            : null;
        HarmonyGraphicsApi? harmonyGraphicsApi = options.Platform == ProjectPackPlatform.HarmonyOS
            ? options.HarmonyDeviceForm == HarmonyDeviceForm.Mobile
                ? HarmonyGraphicsApi.OpenGLES
                : options.HarmonyGraphicsApi
            : null;
        AndroidSigningOptions? androidSigning = null;
        if (options.Platform == ProjectPackPlatform.Android
            && options.AndroidSigning is { } signing)
        {
            androidSigning = signing with
            {
                KeystorePath = Path.GetFullPath(signing.KeystorePath),
            };
        }
        bool requiresPreflight = options.Platform == ProjectPackPlatform.IOS
            || options.Platform == ProjectPackPlatform.HarmonyOS
            || options.Platform == ProjectPackPlatform.Android;
        ProjectPackaging packaging = new(projectPath, options.UseLuac);
        if (requiresPreflight)
        {
            ScriptExecutionResult preflight = await executeScriptAsync(
                scriptPath,
                true,
                options.UseLuac,
                options.EncryptShaders,
                options.EncryptData,
                exportToIPhone,
                exportToHarmonyDevice,
                harmonyDeviceForm,
                harmonyGraphicsApi,
                androidSigning,
                packaging,
                cancellationToken);
            ProjectPackResult? preflightFailure = executionFailure(preflight, options.Platform);
            if (preflightFailure is not null)
                return preflightFailure;
        }

        PluginResult preparation;
        try
        {
            preparation = await operationPipeline.ExecuteAsync(
                ProjectOperationKind.Pack,
                writeOutput,
                cancellationToken,
                packaging);
        }
        catch (OperationCanceledException)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.Cancelled,
                string.Empty);
        }
        if (!preparation.Success)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.PluginPreparationFailed,
                preparation.Error);
        }
        if (cancellationToken.IsCancellationRequested)
            return ProjectPackResult.Failed(ProjectPackFailure.Cancelled, string.Empty);

        if (requiresPreflight)
        {
            ScriptExecutionResult buildPreflight = await executeScriptAsync(
                scriptPath,
                true,
                options.UseLuac,
                options.EncryptShaders,
                options.EncryptData,
                exportToIPhone,
                exportToHarmonyDevice,
                harmonyDeviceForm,
                harmonyGraphicsApi,
                androidSigning,
                packaging,
                cancellationToken);
            ProjectPackResult? buildPreflightFailure = executionFailure(
                buildPreflight,
                options.Platform);
            if (buildPreflightFailure is not null)
                return buildPreflightFailure;
        }

        ScriptExecutionResult execution = await executeScriptAsync(
            scriptPath,
            false,
            options.UseLuac,
            options.EncryptShaders,
            options.EncryptData,
            exportToIPhone,
            exportToHarmonyDevice,
            harmonyDeviceForm,
            harmonyGraphicsApi,
            androidSigning,
            packaging,
            cancellationToken);
        return executionFailure(execution, options.Platform)
            ?? ProjectPackResult.Completed();
    }

    private static ProjectPackResult? validateOptions(
        ProjectPackOptions options,
        string projectFilePath)
    {
        ProjectPackResult? signingFailure = validateAndroidSigning(options);
        if (signingFailure is not null)
            return signingFailure;

        ProjectPackResult? projectFailure = inspectProject(
            projectFilePath,
            out bool isStandalone);
        if (projectFailure is not null)
            return projectFailure;
        if (!isStandalone)
            return null;
        if (options.Platform == ProjectPackPlatform.IOS)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.IOSProjectUnsupported,
                projectFilePath);
        }
        if (options.Platform == ProjectPackPlatform.HarmonyOS)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.HarmonyProjectUnsupported,
                projectFilePath);
        }
        if (options.Platform == ProjectPackPlatform.Android)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.AndroidProjectUnsupported,
                projectFilePath);
        }
        return null;
    }

    private static ProjectPackResult? validateAndroidSigning(ProjectPackOptions options)
    {
        AndroidSigningOptions? signing = options.AndroidSigning;
        if (signing is null)
            return null;
        if (options.Platform != ProjectPackPlatform.Android
            || string.IsNullOrWhiteSpace(signing.KeystorePath)
            || !Path.IsPathFullyQualified(signing.KeystorePath)
            || signing.KeystorePath.IndexOfAny(['\r', '\n']) >= 0
            || !File.Exists(signing.KeystorePath)
            || string.IsNullOrWhiteSpace(signing.KeyAlias)
            || signing.KeyAlias.IndexOfAny(['\r', '\n']) >= 0
            || string.IsNullOrEmpty(signing.KeystorePassword)
            || signing.KeystorePassword.IndexOfAny(['\r', '\n']) >= 0
            || string.IsNullOrEmpty(signing.KeyPassword)
            || signing.KeyPassword.IndexOfAny(['\r', '\n']) >= 0)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.AndroidSigningUnavailable,
                string.Empty);
        }
        return null;
    }

    private static ProjectPackResult? inspectProject(
        string projectFilePath,
        out bool isStandalone)
    {
        isStandalone = true;
        try
        {
            using JsonDocument document = JsonDocument.Parse(
                File.ReadAllText(projectFilePath, Encoding.UTF8));
            if (document.RootElement.ValueKind != JsonValueKind.Object)
            {
                return ProjectPackResult.Failed(
                    ProjectPackFailure.ProjectInvalid,
                    projectFilePath);
            }
            isStandalone = !document.RootElement.TryGetProperty(
                "Cpp",
                out JsonElement cppElement)
            || cppElement.ValueKind != JsonValueKind.True;
            return null;
        }
        catch (IOException exception)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.ProjectInvalid,
                projectFilePath + Environment.NewLine + exception.Message);
        }
        catch (UnauthorizedAccessException exception)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.ProjectInvalid,
                projectFilePath + Environment.NewLine + exception.Message);
        }
        catch (JsonException exception)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.ProjectInvalid,
                projectFilePath + Environment.NewLine + exception.Message);
        }
    }

    private async Task<ScriptExecutionResult> executeScriptAsync(
        string scriptPath,
        bool checkOnly,
        bool useLuac,
        bool encryptShaders,
        bool encryptData,
        bool exportToIPhone,
        bool exportToHarmonyDevice,
        HarmonyDeviceForm? harmonyDeviceForm,
        HarmonyGraphicsApi? harmonyGraphicsApi,
        AndroidSigningOptions? androidSigning,
        ProjectPackaging packaging,
        CancellationToken cancellationToken)
    {
        ProcessStartInfo startInfo = createStartInfo(
            scriptPath,
            checkOnly,
            useLuac,
            encryptShaders,
            encryptData,
            exportToIPhone,
            exportToHarmonyDevice,
            harmonyDeviceForm,
            harmonyGraphicsApi,
            androidSigning,
            packaging);
        using Process process = createProcess(startInfo);
        Task outputTask = Task.CompletedTask;
        Task errorTask = Task.CompletedTask;
        string optionText = (useLuac ? " --compile-lua" : string.Empty)
            + (encryptShaders ? " --encrypt-shaders" : string.Empty)
            + (encryptData ? " --encrypt-data" : string.Empty)
            + (exportToIPhone ? " --export-to-iphone" : string.Empty)
            + (exportToHarmonyDevice ? " --export-to-device" : string.Empty)
            + (harmonyDeviceForm is null
                ? string.Empty
                : " --device-form " + getHarmonyDeviceFormArgument(harmonyDeviceForm.Value))
            + (harmonyGraphicsApi is null
                ? string.Empty
                : " --graphics-api " + getHarmonyGraphicsApiArgument(harmonyGraphicsApi.Value))
            + (androidSigning is not null ? " --sign" : string.Empty);
        writeOutput(checkOnly
            ? $"> {scriptPath} --check{optionText} \"{projectPath}\""
            : $"> {scriptPath}{optionText} \"{projectPath}\"");

        try
        {
            if (!process.Start())
                return ScriptExecutionResult.LaunchFailed(startInfo.FileName);
            process.StandardInput.NewLine = "\n";
            if (androidSigning is not null)
            {
                await process.StandardInput.WriteLineAsync(
                    androidSigning.KeystorePassword).ConfigureAwait(false);
                await process.StandardInput.WriteLineAsync(
                    androidSigning.KeyPassword).ConfigureAwait(false);
                await process.StandardInput.FlushAsync(cancellationToken)
                    .ConfigureAwait(false);
            }
            process.StandardInput.Close();
            outputTask = readOutputAsync(process.StandardOutput, androidSigning);
            errorTask = readOutputAsync(process.StandardError, androidSigning);
            await process.WaitForExitAsync(cancellationToken).ConfigureAwait(false);
            await Task.WhenAll(outputTask, errorTask).ConfigureAwait(false);
            return ScriptExecutionResult.Exited(process.ExitCode);
        }
        catch (OperationCanceledException)
        {
            stopProcess(process);
            await Task.WhenAll(outputTask, errorTask).ConfigureAwait(false);
            return ScriptExecutionResult.CancelledResult();
        }
        catch (Win32Exception exception)
        {
            return ScriptExecutionResult.LaunchFailed(
                redactAndroidSigning(exception.Message, androidSigning));
        }
        catch (InvalidOperationException exception)
        {
            return ScriptExecutionResult.LaunchFailed(
                redactAndroidSigning(exception.Message, androidSigning));
        }
        catch (IOException exception)
        {
            stopProcess(process);
            return ScriptExecutionResult.LaunchFailed(
                redactAndroidSigning(exception.Message, androidSigning));
        }
    }

    private static ProjectPackResult? executionFailure(
        ScriptExecutionResult execution,
        ProjectPackPlatform platform)
    {
        if (execution.Cancelled)
            return ProjectPackResult.Failed(ProjectPackFailure.Cancelled, string.Empty);
        if (execution.LaunchError.Length != 0)
            return ProjectPackResult.Failed(ProjectPackFailure.LaunchFailed, execution.LaunchError);
        if (execution.ExitCode == 0)
            return null;
        if (execution.ExitCode == AppNameUnchangedExitCode)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.AppNameUnchanged,
                execution.ExitCode.ToString());
        }
        if (platform == ProjectPackPlatform.IOS)
        {
            ProjectPackFailure failure = execution.ExitCode switch
            {
                20 => ProjectPackFailure.ToolchainUnavailable,
                21 => ProjectPackFailure.DeviceUnavailable,
                22 => ProjectPackFailure.SigningUnavailable,
                23 => ProjectPackFailure.IOSProjectUnsupported,
                _ => ProjectPackFailure.PackFailed,
            };
            return ProjectPackResult.Failed(failure, execution.ExitCode.ToString());
        }
        if (platform == ProjectPackPlatform.HarmonyOS)
        {
            ProjectPackFailure failure = execution.ExitCode switch
            {
                20 => ProjectPackFailure.HarmonyToolchainUnavailable,
                21 => ProjectPackFailure.HarmonyDeviceUnavailable,
                22 => ProjectPackFailure.HarmonySigningUnavailable,
                23 => ProjectPackFailure.HarmonyProjectUnsupported,
                _ => ProjectPackFailure.PackFailed,
            };
            return ProjectPackResult.Failed(failure, execution.ExitCode.ToString());
        }
        if (platform == ProjectPackPlatform.Android)
        {
            ProjectPackFailure failure = execution.ExitCode switch
            {
                20 => ProjectPackFailure.AndroidToolchainUnavailable,
                22 => ProjectPackFailure.AndroidSigningUnavailable,
                23 => ProjectPackFailure.AndroidProjectUnsupported,
                _ => ProjectPackFailure.PackFailed,
            };
            return ProjectPackResult.Failed(failure, execution.ExitCode.ToString());
        }
        return ProjectPackResult.Failed(
            ProjectPackFailure.PackFailed,
            execution.ExitCode.ToString());
    }

    private ProjectPackResult? validateAppName()
    {
        string entryPath = Path.Combine(projectPath, "Scripts", "Entry.lua");
        if (!File.Exists(entryPath))
            return ProjectPackResult.Failed(ProjectPackFailure.ProjectInvalid, entryPath);

        string source;
        try
        {
            source = File.ReadAllText(entryPath, Encoding.UTF8);
        }
        catch (IOException exception)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.ProjectInvalid,
                entryPath + Environment.NewLine + exception.Message);
        }
        catch (UnauthorizedAccessException exception)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.ProjectInvalid,
                entryPath + Environment.NewLine + exception.Message);
        }

        return defaultAppNamePattern.IsMatch(source)
            ? ProjectPackResult.Failed(ProjectPackFailure.AppNameUnchanged, entryPath)
            : null;
    }

    private ProcessStartInfo createStartInfo(
        string scriptPath,
        bool checkOnly,
        bool useLuac,
        bool encryptShaders,
        bool encryptData,
        bool exportToIPhone,
        bool exportToHarmonyDevice,
        HarmonyDeviceForm? harmonyDeviceForm,
        HarmonyGraphicsApi? harmonyGraphicsApi,
        AndroidSigningOptions? androidSigning,
        ProjectPackaging packaging)
    {
        ProcessStartInfo startInfo = new()
        {
            FileName = OperatingSystem.IsWindows()
                ? Environment.GetEnvironmentVariable("COMSPEC") ?? "cmd.exe"
                : "/bin/bash",
            WorkingDirectory = projectPath,
            UseShellExecute = false,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = utf8,
            StandardErrorEncoding = utf8,
            StandardInputEncoding = utf8,
            CreateNoWindow = true,
        };
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        startInfo.Environment.Remove(CompileLuaDirectoriesEnvironment);
        startInfo.Environment.Remove(ExcludedFilesEnvironment);
        if (packaging.CompileLuaDirectories.Count != 0)
        {
            startInfo.Environment[CompileLuaDirectoriesEnvironment] =
                string.Join('\n', packaging.CompileLuaDirectories);
        }
        if (packaging.ExcludedFiles.Count != 0)
        {
            startInfo.Environment[ExcludedFilesEnvironment] =
                string.Join('\n', packaging.ExcludedFiles);
        }
        if (OperatingSystem.IsWindows())
        {
            startInfo.ArgumentList.Add("/d");
            startInfo.ArgumentList.Add("/s");
            startInfo.ArgumentList.Add("/c");
            startInfo.ArgumentList.Add("call");
        }
        startInfo.ArgumentList.Add(Path.GetFullPath(scriptPath));
        if (checkOnly)
            startInfo.ArgumentList.Add("--check");
        if (useLuac)
            startInfo.ArgumentList.Add("--compile-lua");
        if (encryptShaders)
            startInfo.ArgumentList.Add("--encrypt-shaders");
        if (encryptData)
            startInfo.ArgumentList.Add("--encrypt-data");
        if (exportToIPhone)
            startInfo.ArgumentList.Add("--export-to-iphone");
        if (exportToHarmonyDevice)
            startInfo.ArgumentList.Add("--export-to-device");
        if (harmonyDeviceForm is not null)
        {
            startInfo.ArgumentList.Add("--device-form");
            startInfo.ArgumentList.Add(getHarmonyDeviceFormArgument(harmonyDeviceForm.Value));
        }
        if (harmonyGraphicsApi is not null)
        {
            startInfo.ArgumentList.Add("--graphics-api");
            startInfo.ArgumentList.Add(getHarmonyGraphicsApiArgument(harmonyGraphicsApi.Value));
        }
        if (androidSigning is not null)
        {
            startInfo.ArgumentList.Add("--sign");
            startInfo.ArgumentList.Add("--keystore");
            startInfo.ArgumentList.Add(Path.GetFullPath(androidSigning.KeystorePath));
            startInfo.ArgumentList.Add("--key-alias");
            startInfo.ArgumentList.Add(androidSigning.KeyAlias);
        }
        startInfo.ArgumentList.Add(projectPath);
        return startInfo;
    }

    private static string getHarmonyDeviceFormArgument(HarmonyDeviceForm deviceForm) =>
        deviceForm switch
        {
            HarmonyDeviceForm.Mobile => "mobile",
            HarmonyDeviceForm.TwoInOne => "2in1",
            _ => throw new ArgumentOutOfRangeException(nameof(deviceForm)),
        };

    private static string getHarmonyGraphicsApiArgument(HarmonyGraphicsApi graphicsApi) =>
        graphicsApi switch
        {
            HarmonyGraphicsApi.OpenGL => "opengl",
            HarmonyGraphicsApi.OpenGLES => "opengl-es",
            _ => throw new ArgumentOutOfRangeException(nameof(graphicsApi)),
        };

    private static Process createProcess(ProcessStartInfo startInfo)
    {
        return new Process
        {
            StartInfo = startInfo,
        };
    }

    private static void stopProcess(Process process)
    {
        try
        {
            if (!process.HasExited)
                process.Kill(true);
        }
        catch (InvalidOperationException)
        {
        }
        catch (Win32Exception)
        {
        }
    }

    private async Task readOutputAsync(
        StreamReader reader,
        AndroidSigningOptions? androidSigning)
    {
        while (await reader.ReadLineAsync().ConfigureAwait(false) is { } line)
            writeOutput(redactAndroidSigning(line, androidSigning));
    }

    private static string redactAndroidSigning(
        string text,
        AndroidSigningOptions? androidSigning)
    {
        if (androidSigning is null)
            return text;
        string[] values =
        [
            androidSigning.KeystorePath,
            androidSigning.KeyAlias,
            androidSigning.KeystorePassword,
            androidSigning.KeyPassword,
        ];
        foreach (string value in values
            .Where(value => value.Length != 0)
            .Distinct(StringComparer.Ordinal)
            .OrderByDescending(value => value.Length))
        {
            text = text.Replace(value, "[REDACTED]", StringComparison.Ordinal);
        }
        return text;
    }

    private void writeOutput(string text)
    {
        OutputReceived?.Invoke(this, text + Environment.NewLine);
    }

    private sealed class ProjectPackaging : IProjectPackaging
    {
        private readonly string projectPath;
        private readonly HashSet<string> compileLuaDirectories;
        private readonly HashSet<string> excludedFiles;

        public ProjectPackaging(string projectPath, bool useLuac)
        {
            this.projectPath = projectPath;
            UseLuac = useLuac;
            StringComparer comparer = OperatingSystem.IsWindows()
                ? StringComparer.OrdinalIgnoreCase
                : StringComparer.Ordinal;
            compileLuaDirectories = new HashSet<string>(comparer);
            excludedFiles = new HashSet<string>(comparer);
        }

        public bool UseLuac { get; }

        public IReadOnlyList<string> CompileLuaDirectories =>
            compileLuaDirectories.Order(StringComparer.Ordinal).ToArray();

        public IReadOnlyList<string> ExcludedFiles =>
            excludedFiles.Order(StringComparer.Ordinal).ToArray();

        public void CompileLuaDirectory(string relativePath)
        {
            compileLuaDirectories.Add(normalizeRelativePath(relativePath));
        }

        public void ExcludeFile(string relativePath)
        {
            excludedFiles.Add(normalizeRelativePath(relativePath));
        }

        private string normalizeRelativePath(string relativePath)
        {
            if (string.IsNullOrWhiteSpace(relativePath) ||
                relativePath.IndexOfAny(['\r', '\n']) >= 0 ||
                Path.IsPathFullyQualified(relativePath))
            {
                throw new ArgumentException(
                    "Package paths must be non-empty project-relative paths.",
                    nameof(relativePath));
            }

            string normalizedSeparators = relativePath
                .Replace('\\', Path.DirectorySeparatorChar)
                .Replace('/', Path.DirectorySeparatorChar);
            string fullPath = Path.GetFullPath(
                Path.Combine(projectPath, normalizedSeparators));
            string normalized = Path.GetRelativePath(projectPath, fullPath);
            if (normalized == "." ||
                normalized == ".." ||
                normalized.StartsWith(
                    ".." + Path.DirectorySeparatorChar,
                    StringComparison.Ordinal))
            {
                throw new ArgumentException(
                    "Package paths must remain inside the project.",
                    nameof(relativePath));
            }
            return normalized.Replace(Path.DirectorySeparatorChar, '/');
        }
    }

    private sealed record ScriptExecutionResult(
        int ExitCode,
        bool Cancelled,
        string LaunchError)
    {
        public static ScriptExecutionResult Exited(int exitCode) =>
            new(exitCode, false, string.Empty);

        public static ScriptExecutionResult CancelledResult() =>
            new(-1, true, string.Empty);

        public static ScriptExecutionResult LaunchFailed(string error) =>
            new(-1, false, error);
    }
}
