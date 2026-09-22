using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
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
    ExportFailed,
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
    SaveEncryptionProjectUnsupported,
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
    bool EncryptData,
    bool EncryptSaves,
    bool UseLdPak)
{
    public string Version { get; init; } = "1.0.0";
    public bool Dev { get; init; }
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
    private static readonly UTF8Encoding utf8 = new(false);
    private readonly string projectPath;
    private readonly ProjectOperationPipeline operationPipeline;
    private readonly Func<Action<string>, CancellationToken, Task<ProjectExportResult>> exportProject;

    public ProjectPackService(
        string projectPath,
        Func<Action<string>, CancellationToken, Task<ProjectExportResult>> exportProject,
        IEditorPluginRuntime? pluginRuntime = null)
    {
        if (!Path.IsPathFullyQualified(projectPath))
            throw new ArgumentException(nameof(projectPath));
        this.projectPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
        operationPipeline = new ProjectOperationPipeline(this.projectPath, pluginRuntime);
        this.exportProject = exportProject;
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

        ProjectPackResult? metadataFailure = await validatePackageMetadataAsync(options, cancellationToken);
        if (metadataFailure is not null)
            return metadataFailure;

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
        ProjectPackaging packaging = new(
            projectPath,
            options.UseLuac,
            options.UseLdPak);
        if (requiresPreflight)
        {
            ScriptExecutionResult preflight = await executeScriptAsync(
                scriptPath,
                true,
                options.UseLuac,
                options.EncryptShaders,
                options.EncryptData,
                options.EncryptSaves,
                options.UseLdPak,
                exportToIPhone,
                exportToHarmonyDevice,
                harmonyDeviceForm,
                harmonyGraphicsApi,
                androidSigning,
                packaging,
                options,
                cancellationToken);
            ProjectPackResult? preflightFailure = executionFailure(preflight, options.Platform);
            if (preflightFailure is not null)
                return preflightFailure;
        }

        PluginResult preparation;
        try
        {
            ProjectExportResult exported = await exportProject(writeOutput, cancellationToken);
            if (!exported.Success)
                return ProjectPackResult.Failed(exported.Cancelled ? ProjectPackFailure.Cancelled : ProjectPackFailure.ExportFailed, exported.Detail);
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
                options.EncryptSaves,
                options.UseLdPak,
                exportToIPhone,
                exportToHarmonyDevice,
                harmonyDeviceForm,
                harmonyGraphicsApi,
                androidSigning,
                packaging,
                options,
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
            options.EncryptSaves,
            options.UseLdPak,
            exportToIPhone,
            exportToHarmonyDevice,
            harmonyDeviceForm,
            harmonyGraphicsApi,
            androidSigning,
            packaging,
            options,
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
        if (options.EncryptSaves)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.SaveEncryptionProjectUnsupported,
                projectFilePath);
        }
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
        bool encryptSaves,
        bool useLdPak,
        bool exportToIPhone,
        bool exportToHarmonyDevice,
        HarmonyDeviceForm? harmonyDeviceForm,
        HarmonyGraphicsApi? harmonyGraphicsApi,
        AndroidSigningOptions? androidSigning,
        ProjectPackaging packaging,
        ProjectPackOptions options,
        CancellationToken cancellationToken)
    {
        ProcessStartInfo startInfo = createStartInfo(
            scriptPath,
            checkOnly,
            useLuac,
            encryptShaders,
            encryptData,
            encryptSaves,
            useLdPak,
            exportToIPhone,
            exportToHarmonyDevice,
            harmonyDeviceForm,
            harmonyGraphicsApi,
            androidSigning,
            packaging,
            options);
        using Process process = createProcess(startInfo);
        Task outputTask = Task.CompletedTask;
        Task errorTask = Task.CompletedTask;
        string optionText = " --version " + options.Version
            + (options.Dev ? " --dev" : " --release")
            + (useLuac ? " --compile-lua" : string.Empty)
            + (encryptShaders ? " --encrypt-shaders" : string.Empty)
            + (encryptData ? " --encrypt-data" : string.Empty)
            + (encryptSaves ? " --encrypt-saves" : string.Empty)
            + (useLdPak ? " --use-ldpak" : string.Empty)
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
        if (execution.ExitCode == ProjectToolConstants.AppNameUnchangedExitCode)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.AppNameUnchanged,
                execution.ExitCode.ToString());
        }
        if (platform == ProjectPackPlatform.IOS)
        {
            ProjectPackFailure failure = execution.ExitCode switch
            {
                ProjectToolConstants.ToolchainExitCode => ProjectPackFailure.ToolchainUnavailable,
                ProjectToolConstants.DeviceExitCode => ProjectPackFailure.DeviceUnavailable,
                ProjectToolConstants.SigningExitCode => ProjectPackFailure.SigningUnavailable,
                ProjectToolConstants.ProjectExitCode => ProjectPackFailure.IOSProjectUnsupported,
                _ => ProjectPackFailure.PackFailed,
            };
            return ProjectPackResult.Failed(failure, execution.ExitCode.ToString());
        }
        if (platform == ProjectPackPlatform.HarmonyOS)
        {
            ProjectPackFailure failure = execution.ExitCode switch
            {
                ProjectToolConstants.ToolchainExitCode => ProjectPackFailure.HarmonyToolchainUnavailable,
                ProjectToolConstants.DeviceExitCode => ProjectPackFailure.HarmonyDeviceUnavailable,
                ProjectToolConstants.SigningExitCode => ProjectPackFailure.HarmonySigningUnavailable,
                ProjectToolConstants.ProjectExitCode => ProjectPackFailure.HarmonyProjectUnsupported,
                _ => ProjectPackFailure.PackFailed,
            };
            return ProjectPackResult.Failed(failure, execution.ExitCode.ToString());
        }
        if (platform == ProjectPackPlatform.Android)
        {
            ProjectPackFailure failure = execution.ExitCode switch
            {
                ProjectToolConstants.ToolchainExitCode => ProjectPackFailure.AndroidToolchainUnavailable,
                ProjectToolConstants.SigningExitCode => ProjectPackFailure.AndroidSigningUnavailable,
                ProjectToolConstants.ProjectExitCode => ProjectPackFailure.AndroidProjectUnsupported,
                _ => ProjectPackFailure.PackFailed,
            };
            return ProjectPackResult.Failed(failure, execution.ExitCode.ToString());
        }
        return ProjectPackResult.Failed(
            ProjectPackFailure.PackFailed,
            execution.ExitCode.ToString());
    }

    private async Task<ProjectPackResult?> validatePackageMetadataAsync(
        ProjectPackOptions options,
        CancellationToken cancellationToken)
    {
        ProjectPackageMetadataResult result;
        try
        {
            result = await ProjectPackageMetadataService.ExecuteAsync(
                "check-package-metadata", projectPath, options.Version, options.Dev, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            return ProjectPackResult.Failed(ProjectPackFailure.Cancelled, string.Empty);
        }
        if (result.Output.Length != 0)
            writeOutput(result.Output);
        if (result.Error.Length != 0)
            writeOutput(result.Error);
        if (result.ExitCode == 0)
            return null;
        ProjectPackFailure failure = result.ExitCode switch
        {
            ProjectToolConstants.AppNameUnchangedExitCode => ProjectPackFailure.AppNameUnchanged,
            -1 => ProjectPackFailure.LaunchFailed,
            _ => ProjectPackFailure.ProjectInvalid,
        };
        return ProjectPackResult.Failed(failure,
            result.Error.Length != 0 ? result.Error : result.Output);
    }

    private ProcessStartInfo createStartInfo(
        string scriptPath,
        bool checkOnly,
        bool useLuac,
        bool encryptShaders,
        bool encryptData,
        bool encryptSaves,
        bool useLdPak,
        bool exportToIPhone,
        bool exportToHarmonyDevice,
        HarmonyDeviceForm? harmonyDeviceForm,
        HarmonyGraphicsApi? harmonyGraphicsApi,
        AndroidSigningOptions? androidSigning,
        ProjectPackaging packaging,
        ProjectPackOptions options)
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
        startInfo.Environment.Remove(ProjectToolConstants.CompileLuaDirectoriesEnvironment);
        startInfo.Environment.Remove(ProjectToolConstants.ExcludedFilesEnvironment);
        if (packaging.CompileLuaDirectories.Count != 0)
        {
            startInfo.Environment[ProjectToolConstants.CompileLuaDirectoriesEnvironment] =
                string.Join('\n', packaging.CompileLuaDirectories);
        }
        if (packaging.ExcludedFiles.Count != 0)
        {
            startInfo.Environment[ProjectToolConstants.ExcludedFilesEnvironment] =
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
        startInfo.ArgumentList.Add("--version");
        startInfo.ArgumentList.Add(options.Version);
        startInfo.ArgumentList.Add(options.Dev ? "--dev" : "--release");
        if (checkOnly)
            startInfo.ArgumentList.Add("--check");
        if (useLuac)
            startInfo.ArgumentList.Add("--compile-lua");
        if (encryptShaders)
            startInfo.ArgumentList.Add("--encrypt-shaders");
        if (encryptData)
            startInfo.ArgumentList.Add("--encrypt-data");
        if (encryptSaves)
            startInfo.ArgumentList.Add("--encrypt-saves");
        if (useLdPak)
            startInfo.ArgumentList.Add("--use-ldpak");
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

        public ProjectPackaging(
            string projectPath,
            bool useLuac,
            bool useLdPak)
        {
            this.projectPath = projectPath;
            UseLuac = useLuac;
            UseLdPak = useLdPak;
            StringComparer comparer = OperatingSystem.IsWindows()
                ? StringComparer.OrdinalIgnoreCase
                : StringComparer.Ordinal;
            compileLuaDirectories = new HashSet<string>(comparer);
            excludedFiles = new HashSet<string>(comparer);
        }

        public bool UseLuac { get; }

        public bool UseLdPak { get; }

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
