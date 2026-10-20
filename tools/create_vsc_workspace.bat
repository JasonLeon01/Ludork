@echo off
setlocal DisableDelayedExpansion
set "LUDORK_WORKSPACE_SCRIPT=%~f0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; try { $source = [IO.File]::ReadAllText($env:LUDORK_WORKSPACE_SCRIPT); $marker = '# POWERSHELL' + '_BEGIN'; & ([ScriptBlock]::Create($source.Substring($source.IndexOf($marker) + $marker.Length))) } catch { [Console]::Error.WriteLine('Error: ' + $_.Exception.Message); exit 1 }"
exit /b %errorlevel%

# POWERSHELL_BEGIN
$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$git = (Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$projectRoot = [IO.Directory]::GetParent([IO.Path]::GetDirectoryName($env:LUDORK_WORKSPACE_SCRIPT)).FullName
$branch = $env:LUDORK_WORKSPACE_BRANCH
if ([string]::IsNullOrEmpty($branch)) {
    $branch = 'gh-pages'
}
$localRef = 'refs/heads/' + $branch
$remoteRef = 'refs/remotes/origin/' + $branch
$remoteName = 'origin/' + $branch
$fetchSpec = $localRef + ':' + $remoteRef

function Invoke-Git {
    param([string]$Directory, [string[]]$GitArguments, [switch]$AllowFailure)

    $quotedArguments = foreach ($argument in $GitArguments) {
        '"' + [regex]::Replace([regex]::Replace($argument, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
    }
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $git
    $startInfo.Arguments = $quotedArguments -join ' '
    $startInfo.WorkingDirectory = $Directory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = $utf8
    $startInfo.StandardErrorEncoding = $utf8
    $process = [Diagnostics.Process]::Start($startInfo)
    try {
        $errorTask = $process.StandardError.ReadToEndAsync()
        $output = $process.StandardOutput.ReadToEnd()
        $process.WaitForExit()
        $errorOutput = $errorTask.GetAwaiter().GetResult()
        $result = [pscustomobject]@{ Output = $output; ExitCode = $process.ExitCode }
        if (!$AllowFailure -and $result.ExitCode -ne 0) {
            throw "git $($GitArguments -join ' ') failed (exit $($result.ExitCode)).`n$errorOutput"
        }
        return $result
    } finally {
        $process.Dispose()
    }
}

function Test-SamePath {
    param([string]$Left, [string]$Right)

    return [string]::Equals(
        [IO.Path]::GetFullPath($Left).TrimEnd([char[]]'\/'),
        [IO.Path]::GetFullPath($Right).TrimEnd([char[]]'\/'),
        [StringComparison]::OrdinalIgnoreCase)
}

function Get-WorkspacePath {
    param([string]$Path)

    $projectDrive = [IO.Path]::GetPathRoot($projectRoot)
    $worktreeDrive = [IO.Path]::GetPathRoot($Path)
    if (!(Test-SamePath $projectDrive $worktreeDrive)) {
        return $Path.Replace('\', '/')
    }
    $projectParts = $projectRoot.Substring($projectDrive.Length).Split([char[]]'\/', [StringSplitOptions]::RemoveEmptyEntries)
    $worktreeParts = $Path.Substring($worktreeDrive.Length).Split([char[]]'\/', [StringSplitOptions]::RemoveEmptyEntries)
    $common = 0
    while ($common -lt $projectParts.Length -and $common -lt $worktreeParts.Length -and
        [string]::Equals($projectParts[$common], $worktreeParts[$common], [StringComparison]::OrdinalIgnoreCase)) {
        $common++
    }
    $parts = New-Object 'System.Collections.Generic.List[string]'
    for ($index = $common; $index -lt $projectParts.Length; $index++) {
        $parts.Add('..')
    }
    for ($index = $common; $index -lt $worktreeParts.Length; $index++) {
        $parts.Add($worktreeParts[$index])
    }
    return $parts -join '/'
}

$branchValidation = Invoke-Git $projectRoot @('check-ref-format', '--branch', $branch) -AllowFailure
if ($branchValidation.ExitCode -ne 0 -or
    ![string]::Equals($branchValidation.Output.TrimEnd("`r`n"), $branch, [StringComparison]::Ordinal)) {
    throw "Invalid workspace branch: $branch"
}
$topLevel = (Invoke-Git $projectRoot @('rev-parse', '--show-toplevel')).Output.TrimEnd("`r`n")
if (!(Test-SamePath $projectRoot $topLevel)) {
    throw 'The tools directory must belong to the Git project root.'
}
$commonDirectory = (Invoke-Git $projectRoot @('rev-parse', '--path-format=absolute', '--git-common-dir')).Output.TrimEnd("`r`n")
$worktreeOutput = (Invoke-Git $projectRoot @('worktree', 'list', '--porcelain', '-z')).Output
$matchingWorktrees = @()
$record = @{}
foreach ($field in $worktreeOutput.Split([char]0)) {
    if ($field.Length -eq 0) {
        if ($record.branch -ceq $localRef) {
            $matchingWorktrees += $record.worktree
        }
        $record = @{}
    } elseif ($field.StartsWith('worktree ')) {
        $record.worktree = $field.Substring(9)
    } elseif ($field.StartsWith('branch ')) {
        $record.branch = $field.Substring(7)
    }
}
if ($matchingWorktrees.Count -gt 1) {
    throw "Multiple $branch worktrees are registered; resolve the duplicate checkouts first."
}
$projectName = [IO.Path]::GetFileName($projectRoot)
$worktreePath = if ($matchingWorktrees.Count -eq 1) {
    [IO.Path]::GetFullPath($matchingWorktrees[0])
} else {
    $directoryBranch = [regex]::Replace($branch, '[<>:"/\\|?*]', '-')
    Join-Path ([IO.Directory]::GetParent($projectRoot).FullName) ('Ludork-' + $directoryBranch)
}
if (Test-SamePath $projectRoot $worktreePath) {
    throw "$branch is checked out in the project directory; two different folders are required."
}
if ($matchingWorktrees.Count -eq 1) {
    if (!(Test-Path -LiteralPath $worktreePath -PathType Container)) {
        throw "The registered $branch worktree is missing: $worktreePath"
    }
    $worktreeTopLevel = (Invoke-Git $worktreePath @('rev-parse', '--show-toplevel')).Output.TrimEnd("`r`n")
    $worktreeCommonDirectory = (Invoke-Git $worktreePath @('rev-parse', '--path-format=absolute', '--git-common-dir')).Output.TrimEnd("`r`n")
    $worktreeBranch = (Invoke-Git $worktreePath @('symbolic-ref', '-q', 'HEAD')).Output.TrimEnd("`r`n")
    if (!(Test-SamePath $worktreeTopLevel $worktreePath) -or
        !(Test-SamePath $worktreeCommonDirectory $commonDirectory) -or
        $worktreeBranch -cne $localRef) {
        throw "The registered $branch worktree no longer matches this repository: $worktreePath"
    }
} elseif (Test-Path -LiteralPath $worktreePath) {
    if (!(Test-Path -LiteralPath $worktreePath -PathType Container) -or
        @(Get-ChildItem -LiteralPath $worktreePath -Force | Select-Object -First 1).Count -gt 0) {
        throw "The worktree directory is already occupied: $worktreePath"
    }
}

$workspacePath = Join-Path $projectRoot ($projectName + '.code-workspace')
if (Test-Path -LiteralPath $workspacePath -PathType Container) {
    throw "The workspace path is a directory: $workspacePath"
}
if ([IO.File]::Exists($workspacePath) -and
    ([IO.File]::GetAttributes($workspacePath) -band [IO.FileAttributes]::ReadOnly)) {
    throw "The workspace file is read-only: $workspacePath"
}
$temporaryPath = Join-Path $projectRoot ('.create_vsc_workspace.' + [Guid]::NewGuid().ToString('N') + '.tmp')
try {
    $temporaryFile = [IO.File]::Open($temporaryPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    $temporaryFile.Dispose()
    if ($matchingWorktrees.Count -eq 0) {
        $localBranch = Invoke-Git $projectRoot @('show-ref', '--verify', '--quiet', $localRef) -AllowFailure
        if ($localBranch.ExitCode -gt 1) {
            throw "Could not inspect the local $branch branch."
        }
        if ($localBranch.ExitCode -eq 0) {
            $null = Invoke-Git $projectRoot @('worktree', 'add', '--', $worktreePath, $branch)
        } else {
            $remoteBranch = Invoke-Git $projectRoot @('show-ref', '--verify', '--quiet', $remoteRef) -AllowFailure
            if ($remoteBranch.ExitCode -gt 1) {
                throw "Could not inspect $remoteName."
            }
            if ($remoteBranch.ExitCode -ne 0) {
                $null = Invoke-Git $projectRoot @('fetch', '--no-tags', 'origin', $fetchSpec)
            }
            $null = Invoke-Git $projectRoot @('-c', ('remote.origin.fetch=+' + $fetchSpec), 'worktree', 'add', '--track', '-b', $branch, '--', $worktreePath, $remoteName)
            $upstream = (Invoke-Git $projectRoot @('for-each-ref', '--format=%(upstream)', $localRef)).Output.TrimEnd("`r`n")
            if ($upstream.Length -eq 0) {
                $null = Invoke-Git $projectRoot @('config', '--add', 'remote.origin.fetch', ('+' + $fetchSpec))
            }
        }
    }
    $workspace = [ordered]@{
        folders = @(
            [ordered]@{ path = '.' }
            [ordered]@{ path = (Get-WorkspacePath $worktreePath) }
        )
    }
    $json = ($workspace | ConvertTo-Json -Depth 4) + "`n"
    [IO.File]::WriteAllText($temporaryPath, $json, $utf8)
    if ([IO.File]::Exists($workspacePath)) {
        [IO.File]::Replace($temporaryPath, $workspacePath, [NullString]::Value)
    } else {
        [IO.File]::Move($temporaryPath, $workspacePath)
    }
    [Console]::WriteLine("Created workspace: $workspacePath")
    [Console]::WriteLine("Branch worktree: $worktreePath")
} finally {
    if ([IO.File]::Exists($temporaryPath)) {
        [IO.File]::Delete($temporaryPath)
    }
}
