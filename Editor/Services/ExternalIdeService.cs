using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Ludork.Services;

public enum ExternalIde
{
    VsCode,
    Cursor,
    Clion,
    VisualStudio,
}

public sealed record IdeInitializationResult(bool Succeeded, string Details);

public sealed class ExternalIdeService
{
    private readonly Dictionary<ExternalIde, string> launchers = [];
    private readonly string projectPath;

    public ExternalIdeService(string projectPath, bool includeSourceProjectIdes)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        if (OperatingSystem.IsMacOS())
        {
            registerMacApplication(ExternalIde.VsCode, "Visual Studio Code");
            registerMacApplication(ExternalIde.Cursor, "Cursor");
            if (includeSourceProjectIdes)
                registerMacApplication(ExternalIde.Clion, "CLion");
            return;
        }
        if (!OperatingSystem.IsWindows())
            return;

        string localApplicationData = Environment.GetFolderPath(
            Environment.SpecialFolder.LocalApplicationData);
        string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        string programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
        registerWindowsApplication(
            ExternalIde.VsCode,
            findFirstFile(
                Path.Combine(localApplicationData, "Programs", "Microsoft VS Code", "Code.exe"),
                Path.Combine(programFiles, "Microsoft VS Code", "Code.exe"),
                Path.Combine(programFilesX86, "Microsoft VS Code", "Code.exe"),
                findOnPath("Code.exe")));
        registerWindowsApplication(
            ExternalIde.Cursor,
            findFirstFile(
                Path.Combine(localApplicationData, "Programs", "cursor", "Cursor.exe"),
                Path.Combine(programFiles, "Cursor", "Cursor.exe"),
                findOnPath("Cursor.exe")));
        if (!includeSourceProjectIdes)
            return;
        registerWindowsApplication(ExternalIde.Clion, findClion(localApplicationData, programFiles));
        registerWindowsApplication(ExternalIde.VisualStudio, findVisualStudio(programFiles, programFilesX86));
    }

    public bool IsInstalled(ExternalIde ide)
    {
        return launchers.ContainsKey(ide);
    }

    public bool RequiresInitialization(ExternalIde ide)
    {
        return ide switch
        {
            ExternalIde.Clion => !Directory.Exists(Path.Combine(projectPath, ".idea")),
            ExternalIde.VisualStudio => !Directory.Exists(Path.Combine(projectPath, ".vs"))
                || !File.Exists(Path.Combine(projectPath, "Main.sln")),
            _ => false,
        };
    }

    public async Task<IdeInitializationResult> InitializeAsync(ExternalIde ide)
    {
        string? scriptPath = getInitializationScript(ide);
        if (scriptPath is null || !File.Exists(scriptPath))
            return new IdeInitializationResult(false, scriptPath ?? string.Empty);

        ProcessStartInfo startInfo = createInitializationStartInfo(scriptPath);
        using Process process = new() { StartInfo = startInfo };
        try
        {
            if (!process.Start())
                return new IdeInitializationResult(false, scriptPath);
            Task<string> outputTask = process.StandardOutput.ReadToEndAsync();
            Task<string> errorTask = process.StandardError.ReadToEndAsync();
            await process.WaitForExitAsync();
            string output = await outputTask;
            string error = await errorTask;
            string details = string.IsNullOrWhiteSpace(error) ? output : error;
            return new IdeInitializationResult(process.ExitCode == 0, details.Trim());
        }
        catch (Win32Exception exception)
        {
            return new IdeInitializationResult(false, exception.Message);
        }
        catch (InvalidOperationException exception)
        {
            return new IdeInitializationResult(false, exception.Message);
        }
        catch (IOException exception)
        {
            return new IdeInitializationResult(false, exception.Message);
        }
    }

    public bool Open(ExternalIde ide)
    {
        if (!launchers.TryGetValue(ide, out string? launcher))
            return false;
        string targetPath = ide == ExternalIde.VisualStudio
            ? Path.Combine(projectPath, "Main.sln")
            : projectPath;
        if (ide == ExternalIde.VisualStudio && !File.Exists(targetPath))
            return false;

        ProcessStartInfo startInfo = new()
        {
            FileName = OperatingSystem.IsMacOS() ? "/usr/bin/open" : launcher,
            UseShellExecute = false,
        };
        if (OperatingSystem.IsMacOS())
        {
            startInfo.ArgumentList.Add("-a");
            startInfo.ArgumentList.Add(launcher);
        }
        startInfo.ArgumentList.Add(targetPath);
        try
        {
            Process.Start(startInfo);
            return true;
        }
        catch (Win32Exception)
        {
            return false;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
    }

    private void registerMacApplication(ExternalIde ide, string applicationName)
    {
        if (canResolveMacApplication(applicationName))
            launchers[ide] = applicationName;
    }

    private void registerWindowsApplication(ExternalIde ide, string? executablePath)
    {
        if (executablePath is not null)
            launchers[ide] = executablePath;
    }

    private static bool canResolveMacApplication(string applicationName)
    {
        ProcessStartInfo startInfo = new()
        {
            FileName = "/usr/bin/open",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        startInfo.ArgumentList.Add("-Ra");
        startInfo.ArgumentList.Add(applicationName);
        using Process process = new() { StartInfo = startInfo };
        try
        {
            return process.Start() && process.WaitForExit(3000) && process.ExitCode == 0;
        }
        catch (Win32Exception)
        {
            return false;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
    }

    private static string? findClion(string localApplicationData, string programFiles)
    {
        string? direct = findFirstFile(
            Path.Combine(programFiles, "JetBrains", "CLion", "bin", "clion64.exe"),
            Path.Combine(localApplicationData, "Programs", "CLion", "bin", "clion64.exe"),
            findOnPath("clion64.exe"),
            findOnPath("clion.exe"));
        if (direct is not null)
            return direct;
        string? installed = findVersionedExecutable(
            Path.Combine(programFiles, "JetBrains"),
            "CLion*",
            Path.Combine("bin", "clion64.exe"));
        if (installed is not null)
            return installed;
        installed = findVersionedExecutable(
            Path.Combine(localApplicationData, "Programs"),
            "CLion*",
            Path.Combine("bin", "clion64.exe"));
        if (installed is not null)
            return installed;
        return findRecursively(
            Path.Combine(localApplicationData, "JetBrains", "Toolbox", "apps", "CLion"),
            "clion64.exe");
    }

    private static string? findVisualStudio(string programFiles, string programFilesX86)
    {
        string? discovered = findWithVsWhere(programFilesX86);
        if (discovered is not null)
            return discovered;
        string visualStudioRoot = Path.Combine(programFiles, "Microsoft Visual Studio", "2022");
        foreach (string edition in new[] { "Enterprise", "Professional", "Community" })
        {
            string path = Path.Combine(visualStudioRoot, edition, "Common7", "IDE", "devenv.exe");
            if (File.Exists(path))
                return path;
        }
        return findOnPath("devenv.exe");
    }

    private static string? findWithVsWhere(string programFilesX86)
    {
        string vsWhere = Path.Combine(
            programFilesX86,
            "Microsoft Visual Studio",
            "Installer",
            "vswhere.exe");
        if (!File.Exists(vsWhere))
            return null;
        ProcessStartInfo startInfo = new()
        {
            FileName = vsWhere,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        startInfo.ArgumentList.Add("-latest");
        startInfo.ArgumentList.Add("-products");
        startInfo.ArgumentList.Add("*");
        startInfo.ArgumentList.Add("-version");
        startInfo.ArgumentList.Add("[17.0,18.0)");
        startInfo.ArgumentList.Add("-find");
        startInfo.ArgumentList.Add(Path.Combine("Common7", "IDE", "devenv.exe"));
        using Process process = new() { StartInfo = startInfo };
        try
        {
            if (!process.Start())
                return null;
            string output = process.StandardOutput.ReadToEnd();
            process.WaitForExit();
            return process.ExitCode == 0
                ? output.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries)
                    .FirstOrDefault(File.Exists)
                : null;
        }
        catch (Win32Exception)
        {
            return null;
        }
        catch (InvalidOperationException)
        {
            return null;
        }
    }

    private static string? findVersionedExecutable(string root, string pattern, string relativePath)
    {
        if (!Directory.Exists(root))
            return null;
        try
        {
            return Directory.EnumerateDirectories(root, pattern)
                .OrderByDescending(path => path, StringComparer.OrdinalIgnoreCase)
                .Select(path => Path.Combine(path, relativePath))
                .FirstOrDefault(File.Exists);
        }
        catch (IOException)
        {
            return null;
        }
        catch (UnauthorizedAccessException)
        {
            return null;
        }
    }

    private static string? findRecursively(string root, string fileName)
    {
        if (!Directory.Exists(root))
            return null;
        try
        {
            return Directory.EnumerateFiles(root, fileName, SearchOption.AllDirectories)
                .OrderByDescending(path => path, StringComparer.OrdinalIgnoreCase)
                .FirstOrDefault();
        }
        catch (IOException)
        {
            return null;
        }
        catch (UnauthorizedAccessException)
        {
            return null;
        }
    }

    private static string? findOnPath(string fileName)
    {
        string? pathValue = Environment.GetEnvironmentVariable("PATH");
        if (string.IsNullOrWhiteSpace(pathValue))
            return null;
        foreach (string directory in pathValue.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            string candidate = Path.Combine(directory.Trim('"'), fileName);
            if (File.Exists(candidate))
                return candidate;
        }
        return null;
    }

    private static string? findFirstFile(params string?[] paths)
    {
        return paths.FirstOrDefault(path => path is not null && File.Exists(path));
    }

    private string? getInitializationScript(ExternalIde ide)
    {
        if (ide == ExternalIde.Clion)
        {
            return Path.Combine(
                projectPath,
                OperatingSystem.IsWindows() ? "generate_clion.bat" : "generate_clion.sh");
        }
        return ide == ExternalIde.VisualStudio && OperatingSystem.IsWindows()
            ? Path.Combine(projectPath, "generate_vs2022.bat")
            : null;
    }

    private ProcessStartInfo createInitializationStartInfo(string scriptPath)
    {
        ProcessStartInfo startInfo = new()
        {
            FileName = OperatingSystem.IsWindows()
                ? Environment.GetEnvironmentVariable("ComSpec") ?? "cmd.exe"
                : "/bin/sh",
            WorkingDirectory = projectPath,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
            CreateNoWindow = true,
        };
        if (OperatingSystem.IsWindows())
        {
            startInfo.Arguments = $"/d /s /c \"\"{scriptPath}\"\"";
        }
        else
        {
            startInfo.ArgumentList.Add(scriptPath);
        }
        return startInfo;
    }
}
