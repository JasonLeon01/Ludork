using Avalonia.Controls;
using Ludork.Composition;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.ComponentModel;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Coordination;

internal sealed class ProjectCompilerCoordinator(
    Window owner, EditorProjectSession session, ProjectOperationCoordinator operations)
{
    public async Task<bool> ApplyAsync(Window dialogOwner, string? target)
        => await applyAsync(dialogOwner, target, false, CancellationToken.None);

    public async Task<bool> EnsureAsync(CancellationToken token)
    {
        if (!OperatingSystem.IsWindows() || session.ProjectConfig.IsStandalone)
            return true;
        try
        {
            return await applyAsync(owner, session.ProjectConfig.CompilerPath, true, token);
        }
        catch (Exception exception) when (isExpected(exception))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("COMPILER_CHANGE"), exception.Message);
            return false;
        }
    }

    private async Task<bool> applyAsync(Window dialogOwner, string? target, bool operationOwned, CancellationToken token)
    {
        if (!OperatingSystem.IsWindows() || session.ProjectConfig.IsStandalone)
            return true;
        try
        {
            string? previous = session.ProjectConfig.CompilerPath;
            await WindowsCompilerService.ValidateAsync(target, token);
            WindowsCompilerStatus status = await WindowsCompilerService.StatusAsync(session.ProjectPath, target, token);
            bool changed = !WindowsCompilerService.SameCompiler(previous, target);
            if (!changed && !status.NeedsCleanup)
                return true;
            string original = status.NeedsCleanup ? status.CurrentCompiler ?? "MSVC" : previous ?? "MSVC";
            if (!await ProjectCompilerDialog.ConfirmChangeAsync(dialogOwner, original, target, token))
                return false;
            async Task<bool> switchAsync()
            {
                await WindowsCompilerService.ValidateAsync(target);
                if (!WindowsCompilerService.SameCompiler(previous, session.ProjectConfig.CompilerPath))
                    throw new IOException(LocaleService.Get("COMPILER_PROJECT_CHANGED"));
                await using IAsyncDisposable suspension = await session.PreviewService.Runtime.SuspendAsync();
                try
                {
                    await WindowsCompilerService.SwitchAsync(session.ProjectPath, target, previous);
                    return true;
                }
                finally
                {
                    session.ProjectRunner.NativeBuildState.RefreshAvailability();
                }
            }
            bool switched = operationOwned ? await switchAsync() : await operations.ChangeCompilerAsync(switchAsync);
            if (!switched)
                await AlertDialog.ShowAsync(dialogOwner, LocaleService.Get("COMPILER_CHANGE"),
                    LocaleService.Get("COMPILER_SWITCH_BUSY"));
            return switched;
        }
        catch (Exception exception) when (isExpected(exception))
        {
            await AlertDialog.ShowAsync(dialogOwner, LocaleService.Get("COMPILER_CHANGE"),
                LocaleService.Get("COMPILER_SWITCH_FAILED") + Environment.NewLine + exception.Message);
            return false;
        }
    }

    private static bool isExpected(Exception exception) => exception is IOException or UnauthorizedAccessException
        or InvalidOperationException or ArgumentException or Win32Exception or JsonException;
}
