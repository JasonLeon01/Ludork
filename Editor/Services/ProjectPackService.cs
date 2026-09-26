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
    MacOSSigningUnavailable,
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

public sealed record MacOSNotarizationOptions(
    string AppleId,
    string TeamId,
    string AppSpecificPassword,
    string KeyPath,
    string KeyId,
    string KeyIssuer)
{
    public bool UsesApiKey => KeyPath.Length != 0;
    public override string ToString() =>
        nameof(MacOSNotarizationOptions) + " { Redacted }";
}

public sealed record MacOSSigningOptions(
    string SigningIdentity,
    string CertificatePath,
    string CertificatePassword,
    MacOSNotarizationOptions? Notarization)
{
    public override string ToString() =>
        nameof(MacOSSigningOptions) + " { Redacted }";
}

public sealed record IOSSigningOptions(
    string TeamId,
    string CertificatePath,
    string CertificatePassword,
    string ProvisioningProfilePath,
    string SigningIdentity)
{
    public override string ToString() =>
        nameof(IOSSigningOptions) + " { Redacted }";
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
    public HarmonySigningOptions? HarmonySigning { get; init; }
    public MacOSSigningOptions? MacOSSigning { get; init; }
    public IOSSigningOptions? IOSSigning { get; init; }
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
        PackSigning signing = createSigning(options);
        bool requiresPreflight = options.Platform == ProjectPackPlatform.IOS
            || options.Platform == ProjectPackPlatform.HarmonyOS
            || options.Platform == ProjectPackPlatform.Android
            || (options.Platform == ProjectPackPlatform.MacOS && signing.MacOS is not null);
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
                signing,
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
                signing,
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
            signing,
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
        ProjectPackResult? signingFailure = validateSigning(options, Path.GetDirectoryName(projectFilePath));
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

    private static ProjectPackResult? validateSigning(ProjectPackOptions options, string? projectPath)
    {
        if (options.Platform == ProjectPackPlatform.HarmonyOS)
        {
            HarmonySigningOptions? harmony = options.HarmonySigning;
            if ((options.ExportToHarmonyDevice && harmony is null)
                || (harmony is not null
                    && (!HarmonySigningInput.IsReadableFile(harmony.KeystorePath)
                        || !HarmonySigningInput.IsReadableFile(harmony.CertificatePath)
                        || !HarmonySigningInput.IsReadableFile(harmony.ProfilePath)
                        || !HarmonySigningInput.IsOutsideProject(harmony.KeystorePath, projectPath)
                        || !HarmonySigningInput.IsOutsideProject(harmony.CertificatePath, projectPath)
                        || !HarmonySigningInput.IsOutsideProject(harmony.ProfilePath, projectPath)
                        || string.IsNullOrWhiteSpace(harmony.KeyAlias)
                        || HarmonySigningInput.HasLineBreak(harmony.KeyAlias)
                        || !HarmonySigningInput.IsNonEmptySingleLine(harmony.KeystorePassword)
                        || !HarmonySigningInput.IsNonEmptySingleLine(harmony.KeyPassword))))
            {
                return ProjectPackResult.Failed(
                    ProjectPackFailure.HarmonySigningUnavailable, string.Empty);
            }
            return null;
        }
        if (options.Platform == ProjectPackPlatform.Android)
        {
            if (options.AndroidSigning is { } android
                && (string.IsNullOrWhiteSpace(android.KeystorePath)
                    || !Path.IsPathFullyQualified(android.KeystorePath)
                    || android.KeystorePath.IndexOfAny(['\r', '\n']) >= 0
                    || !File.Exists(android.KeystorePath)
                    || string.IsNullOrWhiteSpace(android.KeyAlias)
                    || android.KeyAlias.IndexOfAny(['\r', '\n']) >= 0
                    || !AppleSigningInput.IsNonEmptySingleLine(android.KeystorePassword)
                    || !AppleSigningInput.IsNonEmptySingleLine(android.KeyPassword)))
            {
                return ProjectPackResult.Failed(
                    ProjectPackFailure.AndroidSigningUnavailable,
                    string.Empty);
            }
            return null;
        }
        if (options.Platform == ProjectPackPlatform.MacOS)
            return validateMacOSSigning(options.MacOSSigning);
        if (options.Platform == ProjectPackPlatform.IOS)
            return validateIOSSigning(options.IOSSigning);
        return null;
    }

    private static ProjectPackResult? validateMacOSSigning(MacOSSigningOptions? signing)
    {
        if (signing is null)
            return null;
        if (signing.SigningIdentity.IndexOfAny(['\r', '\n']) >= 0
            || !AppleSigningInput.IsOptionalReadableFile(signing.CertificatePath)
            || (signing.CertificatePath.Length != 0
                && !AppleSigningInput.IsNonEmptySingleLine(signing.CertificatePassword)))
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.MacOSSigningUnavailable,
                string.Empty);
        }
        if (signing.Notarization is not { } notarization)
            return null;
        if (signing.SigningIdentity.Length == 0 && signing.CertificatePath.Length == 0)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.MacOSSigningUnavailable,
                string.Empty);
        }
        bool validCredentials = notarization.UsesApiKey
            ? AppleSigningInput.IsReadableFile(notarization.KeyPath)
                && AppleSigningInput.IsNonEmptySingleLine(notarization.KeyId)
                && AppleSigningInput.IsNonEmptySingleLine(notarization.KeyIssuer)
            : AppleSigningInput.IsNonEmptySingleLine(notarization.AppleId)
                && AppleSigningInput.IsValidTeamId(notarization.TeamId)
                && AppleSigningInput.IsNonEmptySingleLine(notarization.AppSpecificPassword);
        if (!validCredentials)
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.MacOSSigningUnavailable,
                string.Empty);
        }
        return null;
    }

    private static ProjectPackResult? validateIOSSigning(IOSSigningOptions? signing)
    {
        if (signing is null)
            return null;
        bool hasCertificate = signing.CertificatePath.Length != 0;
        bool hasProfile = signing.ProvisioningProfilePath.Length != 0;
        if (hasCertificate != hasProfile
            || signing.SigningIdentity.IndexOfAny(['\r', '\n']) >= 0
            || (signing.TeamId.Length != 0 && !AppleSigningInput.IsValidTeamId(signing.TeamId)))
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.SigningUnavailable,
                string.Empty);
        }
        if (hasCertificate
            && (!AppleSigningInput.IsReadableFile(signing.CertificatePath)
                || !AppleSigningInput.IsReadableFile(signing.ProvisioningProfilePath)
                || !AppleSigningInput.IsNonEmptySingleLine(signing.CertificatePassword)))
        {
            return ProjectPackResult.Failed(
                ProjectPackFailure.SigningUnavailable,
                string.Empty);
        }
        return null;
    }

    private static PackSigning createSigning(ProjectPackOptions options)
    {
        AndroidSigningOptions? android = null;
        if (options.Platform == ProjectPackPlatform.Android
            && options.AndroidSigning is { } androidSigning)
        {
            android = androidSigning with
            {
                KeystorePath = Path.GetFullPath(androidSigning.KeystorePath),
            };
        }
        MacOSSigningOptions? macOS = null;
        if (options.Platform == ProjectPackPlatform.MacOS
            && options.MacOSSigning is { } macOSSigning)
        {
            macOS = macOSSigning with
            {
                CertificatePath = normalizeOptionalPath(macOSSigning.CertificatePath),
                Notarization = macOSSigning.Notarization is { } notarization
                    ? notarization with
                    {
                        KeyPath = normalizeOptionalPath(notarization.KeyPath),
                    }
                    : null,
            };
        }
        IOSSigningOptions? ios = null;
        if (options.Platform == ProjectPackPlatform.IOS
            && options.IOSSigning is { } iosSigning)
        {
            ios = iosSigning with
            {
                CertificatePath = normalizeOptionalPath(iosSigning.CertificatePath),
                ProvisioningProfilePath =
                    normalizeOptionalPath(iosSigning.ProvisioningProfilePath),
            };
        }
        HarmonySigningOptions? harmony = null;
        if (options.Platform == ProjectPackPlatform.HarmonyOS
            && options.HarmonySigning is { } harmonySigning)
        {
            harmony = harmonySigning with
            {
                KeystorePath = Path.GetFullPath(harmonySigning.KeystorePath),
                CertificatePath = Path.GetFullPath(harmonySigning.CertificatePath),
                ProfilePath = Path.GetFullPath(harmonySigning.ProfilePath),
            };
        }
        return new PackSigning(android, macOS, ios, harmony);
    }

    private static string normalizeOptionalPath(string path) =>
        path.Length == 0 ? string.Empty : Path.GetFullPath(path);

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
        PackSigning signing,
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
            signing,
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
            + (signing.Android is not null || signing.Harmony is not null ? " --sign" : string.Empty);
        writeOutput(checkOnly
            ? $"> {scriptPath} --check{optionText} \"{projectPath}\""
            : $"> {scriptPath}{optionText} \"{projectPath}\"");

        try
        {
            if (!process.Start())
                return ScriptExecutionResult.LaunchFailed(startInfo.FileName);
            process.StandardInput.NewLine = "\n";
            foreach (string secret in signing.Secrets())
            {
                await process.StandardInput.WriteLineAsync(secret).ConfigureAwait(false);
            }
            await process.StandardInput.FlushAsync(cancellationToken).ConfigureAwait(false);
            process.StandardInput.Close();
            outputTask = readOutputAsync(process.StandardOutput, signing);
            errorTask = readOutputAsync(process.StandardError, signing);
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
                redactSigning(exception.Message, signing));
        }
        catch (InvalidOperationException exception)
        {
            return ScriptExecutionResult.LaunchFailed(
                redactSigning(exception.Message, signing));
        }
        catch (IOException exception)
        {
            stopProcess(process);
            return ScriptExecutionResult.LaunchFailed(
                redactSigning(exception.Message, signing));
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
        if (platform == ProjectPackPlatform.MacOS)
        {
            ProjectPackFailure failure = execution.ExitCode switch
            {
                ProjectToolConstants.ToolchainExitCode => ProjectPackFailure.ToolchainUnavailable,
                ProjectToolConstants.SigningExitCode => ProjectPackFailure.MacOSSigningUnavailable,
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
        PackSigning signing,
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
        if (signing.Android is { } androidSigning)
        {
            startInfo.ArgumentList.Add("--sign");
            startInfo.ArgumentList.Add("--keystore");
            startInfo.ArgumentList.Add(Path.GetFullPath(androidSigning.KeystorePath));
            startInfo.ArgumentList.Add("--key-alias");
            startInfo.ArgumentList.Add(androidSigning.KeyAlias);
        }
        if (signing.Harmony is { } harmonySigning)
        {
            startInfo.ArgumentList.Add("--sign");
            startInfo.ArgumentList.Add("--keystore");
            startInfo.ArgumentList.Add(harmonySigning.KeystorePath);
            startInfo.ArgumentList.Add("--certificate");
            startInfo.ArgumentList.Add(harmonySigning.CertificatePath);
            startInfo.ArgumentList.Add("--profile");
            startInfo.ArgumentList.Add(harmonySigning.ProfilePath);
            startInfo.ArgumentList.Add("--key-alias");
            startInfo.ArgumentList.Add(harmonySigning.KeyAlias);
        }
        if (signing.MacOS is { } macOSSigning)
        {
            startInfo.ArgumentList.Add("--ignore-environment");
            if (macOSSigning.SigningIdentity.Length != 0)
            {
                startInfo.ArgumentList.Add("--signing-identity");
                startInfo.ArgumentList.Add(macOSSigning.SigningIdentity);
            }
            if (macOSSigning.CertificatePath.Length != 0)
            {
                startInfo.ArgumentList.Add("--certificate");
                startInfo.ArgumentList.Add(macOSSigning.CertificatePath);
            }
            if (macOSSigning.Notarization is { } notarization)
            {
                startInfo.ArgumentList.Add("--notarize");
                if (notarization.UsesApiKey)
                {
                    startInfo.ArgumentList.Add("--notary-key");
                    startInfo.ArgumentList.Add(notarization.KeyPath);
                    startInfo.ArgumentList.Add("--notary-key-id");
                    startInfo.ArgumentList.Add(notarization.KeyId);
                    startInfo.ArgumentList.Add("--notary-key-issuer");
                    startInfo.ArgumentList.Add(notarization.KeyIssuer);
                }
                else
                {
                    startInfo.ArgumentList.Add("--notary-apple-id");
                    startInfo.ArgumentList.Add(notarization.AppleId);
                    startInfo.ArgumentList.Add("--notary-team-id");
                    startInfo.ArgumentList.Add(notarization.TeamId);
                }
            }
        }
        if (signing.IOS is { } iosSigning)
        {
            startInfo.ArgumentList.Add("--ignore-environment");
            if (iosSigning.TeamId.Length != 0)
            {
                startInfo.ArgumentList.Add("--team-id");
                startInfo.ArgumentList.Add(iosSigning.TeamId);
            }
            if (iosSigning.CertificatePath.Length != 0)
            {
                startInfo.ArgumentList.Add("--certificate");
                startInfo.ArgumentList.Add(iosSigning.CertificatePath);
            }
            if (iosSigning.ProvisioningProfilePath.Length != 0)
            {
                startInfo.ArgumentList.Add("--provisioning-profile");
                startInfo.ArgumentList.Add(iosSigning.ProvisioningProfilePath);
            }
            if (iosSigning.SigningIdentity.Length != 0)
            {
                startInfo.ArgumentList.Add("--signing-identity");
                startInfo.ArgumentList.Add(iosSigning.SigningIdentity);
            }
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
        PackSigning signing)
    {
        while (await reader.ReadLineAsync().ConfigureAwait(false) is { } line)
            writeOutput(redactSigning(line, signing));
    }

    private static string redactSigning(string text, PackSigning signing)
    {
        foreach (string value in signing.SensitiveValues()
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

    private sealed record PackSigning(
        AndroidSigningOptions? Android,
        MacOSSigningOptions? MacOS,
        IOSSigningOptions? IOS,
        HarmonySigningOptions? Harmony)
    {
        public IEnumerable<string> Secrets()
        {
            if (Harmony is { } harmony)
            {
                yield return harmony.KeystorePassword;
                yield return harmony.KeyPassword;
            }
            if (Android is { } android)
            {
                yield return android.KeystorePassword;
                yield return android.KeyPassword;
            }
            if (MacOS is { } macOS)
            {
                if (macOS.CertificatePath.Length != 0)
                    yield return macOS.CertificatePassword;
                if (macOS.Notarization is { } notarization && !notarization.UsesApiKey)
                    yield return notarization.AppSpecificPassword;
            }
            if (IOS is { } ios && ios.CertificatePath.Length != 0)
                yield return ios.CertificatePassword;
        }

        public IEnumerable<string> SensitiveValues()
        {
            if (Harmony is { } harmony)
            {
                yield return harmony.KeystorePath;
                yield return harmony.CertificatePath;
                yield return harmony.ProfilePath;
                yield return harmony.KeyAlias;
                yield return harmony.KeystorePassword;
                yield return harmony.KeyPassword;
            }
            if (Android is { } android)
            {
                yield return android.KeystorePath;
                yield return android.KeyAlias;
                yield return android.KeystorePassword;
                yield return android.KeyPassword;
            }
            if (MacOS is { } macOS)
            {
                yield return macOS.CertificatePath;
                yield return macOS.CertificatePassword;
                if (macOS.Notarization is { } notarization)
                {
                    yield return notarization.AppleId;
                    yield return notarization.AppSpecificPassword;
                    yield return notarization.KeyPath;
                    yield return notarization.KeyId;
                    yield return notarization.KeyIssuer;
                }
            }
            if (IOS is { } ios)
            {
                yield return ios.CertificatePath;
                yield return ios.CertificatePassword;
                yield return ios.ProvisioningProfilePath;
            }
        }
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
