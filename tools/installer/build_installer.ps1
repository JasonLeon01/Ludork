param(
    [ValidateSet('msi', 'exe')]
    [string]$Format,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildInfo = Join-Path $root 'obj\editor-package-metadata\BuildInfo.json'
$outputPath = $null
for ($index = 0; $index -lt $args.Count; $index++) {
    if ($args[$index] -eq '--build-info') {
        $index++
        if ($index -ge $args.Count) { throw '--build-info requires a file path.' }
        $buildInfo = [IO.Path]::GetFullPath([IO.Path]::Combine($root, $args[$index]))
    } elseif ($args[$index].StartsWith('--') -or $null -ne $outputPath) {
        throw 'Usage: pack_editor_<msi|exe>.bat [--build-info <file>] [output.<msi|exe>]'
    } else {
        $outputPath = [IO.Path]::GetFullPath([IO.Path]::Combine($root, $args[$index]))
    }
}
if ([string]::IsNullOrWhiteSpace($Format)) { throw '-Format msi or exe is required.' }
if ($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitOperatingSystem) {
    throw 'Windows x64 is required to build Ludork installers.'
}

function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE." }
}

function Require-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file was not found: $Path"
    }
}

function Build-NativeActions([string]$Destination, [string]$Architecture = 'x64') {
    Invoke-Checked 'cmake.exe' @('-S', (Join-Path $PSScriptRoot 'native'), '-B', $Destination,
        '-A', $Architecture, '-DCMAKE_BUILD_TYPE=Release')
    Invoke-Checked 'cmake.exe' @('--build', $Destination, '--config', 'Release', '--target', 'LudorkInstallerActions')
    $nativeDll = Join-Path $Destination 'bin\Release\LudorkInstallerActions.dll'
    Require-File $nativeDll
}

function Write-License([string]$Destination) {
    $notice = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'InstallerNotice.en.txt'))
    $license = [IO.File]::ReadAllText((Join-Path $root 'LICENSE.md'))
    $text = $notice.TrimEnd() + "`r`n`r`n" + $license
    [IO.File]::WriteAllText((Join-Path $Destination 'License.en.txt'), $text,
        [Text.UTF8Encoding]::new($true))
    $rtf = $text.Replace('\', '\\').Replace('{', '\{').Replace('}', '\}')
    $rtf = $rtf -replace '\r?\n', '\par '
    [IO.File]::WriteAllText((Join-Path $Destination 'License.en.rtf'),
        '{\rtf1\ansi\deff0{\fonttbl{\f0 Segoe UI;}}\f0\fs18 ' + $rtf + '}',
        [Text.Encoding]::ASCII)
}

function Write-PayloadManifest([string]$SourceDirectory, [string]$Destination) {
    $payloadFiles = @(
        Get-ChildItem -LiteralPath $SourceDirectory -Recurse -File -Force | ForEach-Object {
            $_.FullName.Substring($SourceDirectory.Length + 1)
        }
        'LudorkProject.ico'
        'tools\InstallerPayload.txt'
        if ($Format -eq 'exe') { 'tools\LudorkUninstallActions.dll' }
    ) | Sort-Object -Unique
    [IO.File]::WriteAllLines((Join-Path $Destination 'InstallerPayload.txt'), $payloadFiles, [Text.UTF8Encoding]::new($true))
}

function Ensure-Inno {
    $innoVersion = $versions['INNO_SETUP_VERSION']
    $innoRoot = Join-Path $root '.tools\inno'
    $innoDir = Join-Path $innoRoot $innoVersion
    $compiler = Join-Path $innoDir 'ISCC.exe'
    if (-not (Test-Path -LiteralPath $compiler)) {
        [void](New-Item -ItemType Directory -Path $innoRoot -Force)
        $download = Join-Path $innoRoot "innosetup-$innoVersion.exe"
        $tag = $innoVersion.Replace('.', '_')
        $url = "https://github.com/jrsoftware/issrc/releases/download/is-$tag/innosetup-$innoVersion.exe"
        if (-not (Test-Path -LiteralPath $download)) {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $url -OutFile $download -UseBasicParsing
        }
        if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne $versions['INNO_SETUP_SHA256']) {
            Remove-Item -LiteralPath $download -Force
            throw 'The Inno Setup download did not match its pinned SHA-256 digest.'
        }
        $process = Start-Process -FilePath $download -ArgumentList @('/CURRENTUSER', '/VERYSILENT',
            '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/NOICONS', '/TASKS=', "/DIR=`"$innoDir`"") -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Inno Setup installation failed: $($process.ExitCode)." }
    }
    Require-File $compiler
    return $compiler
}

$versions = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $root 'versions.conf')) {
    if ($line -match '^([A-Z0-9_]+)=(.+)$') { $versions[$Matches[1]] = $Matches[2] }
}
foreach ($key in @('EDITOR_VERSION', 'WIX_VERSION', 'INNO_SETUP_VERSION', 'INNO_SETUP_SHA256')) {
    if (-not $versions.ContainsKey($key)) { throw "Missing $key in versions.conf." }
}
if ($versions['INNO_SETUP_SHA256'] -notmatch '^[0-9a-f]{64}$') {
    throw 'INNO_SETUP_SHA256 must be a full SHA-256 digest.'
}

[xml]$definition = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'Ludork.wxs')
foreach ($script in Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1') {
    $tokens = $null
    $parseErrors = $null
    [void][Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count -ne 0) { throw ($parseErrors | Out-String) }
}

$work = Join-Path $root "obj\installer\$Format"
if ($ValidateOnly) {
    Build-NativeActions (Join-Path $work 'native-x64')
    Build-NativeActions (Join-Path $work 'native-uninstall') 'Win32'
    $validationWork = Join-Path $root 'obj\installer\validate'
    [void](New-Item -ItemType Directory -Path $validationWork -Force)
    try {
        Write-License $validationWork
        Write-PayloadManifest $PSScriptRoot $validationWork
        $compiler = Ensure-Inno
        Invoke-Checked $compiler @('/O-', "/DCompilerVersion=$($versions['INNO_SETUP_VERSION'])", "/DProductVersion=$($versions['EDITOR_VERSION'])",
            "/DDistDir=$PSScriptRoot", "/DWorkDir=$validationWork", "/DAssetsDir=$root\Editor\Assets",
            "/DUninstallActions=$work\native-uninstall\bin\Release\LudorkInstallerActions.dll",
            (Join-Path $PSScriptRoot 'Ludork.iss'))
    } finally {
        Remove-Item -LiteralPath $validationWork -Recurse -Force
    }
    Write-Host 'PowerShell and WiX XML parsed; native custom actions built; real Inno definition compiled with output disabled. Product payload validation runs during release packaging.'
    exit 0
}

$dist = Join-Path $root 'dist'
$scriptTools = Join-Path $root '.tools\ScriptTools\ScriptTools.exe'
Require-File $scriptTools
Require-File $buildInfo
Require-File (Join-Path $dist 'Ludork.exe')
Require-File (Join-Path $dist 'tools\ScriptTools\ScriptTools.exe')
Require-File (Join-Path $dist 'tools\OfficialPlugins\plugins.json')
Invoke-Checked (Join-Path $root 'tools\validate_editor_windows_layout.bat') @($dist, '--release')
$dev = & $scriptTools packaging-constants build-info $buildInfo --field dev
if ($LASTEXITCODE -ne 0 -or $dev -ne 'false') {
    throw 'Windows installers require a release dist. Run tools\pack_editor.bat --release first.'
}
$productVersion = & $scriptTools packaging-constants build-info $buildInfo --field version
if ($LASTEXITCODE -ne 0 -or $productVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "Cannot read a valid packaged editor version from $buildInfo."
}
Invoke-Checked (Join-Path $dist 'Binaries\Ludork.exe') @('--check-build-info', $buildInfo)
Invoke-Checked $scriptTools @('editor-build-metadata', 'validate-payload', $dist, '--build-info', $buildInfo)
Invoke-Checked $scriptTools @('editor-official-plugins', 'validate', (Join-Path $dist 'tools\OfficialPlugins'))
if (Test-Path -LiteralPath (Join-Path $dist 'Templates')) {
    throw 'A release installer must not contain embedded development templates.'
}
foreach ($file in Get-ChildItem -LiteralPath $dist -Recurse -File -Force) {
    if ($file.Extension -eq '.pdb' -or $file.Name.EndsWith('.resources.dll')) {
        throw "Forbidden editor package file: $($file.FullName)"
    }
}
foreach ($forbidden in @('tools\installer', 'tools\pack_editor_msi.bat', 'tools\pack_editor_exe.bat')) {
    if (Test-Path -LiteralPath (Join-Path $dist $forbidden)) {
        throw "Installer build inputs must not be present in dist: $forbidden"
    }
}
if ($null -eq $outputPath) {
    $outputPath = Join-Path $root "obj\installer\output\Ludork-$productVersion-windows-x64.$Format"
}
if ([IO.Path]::GetExtension($outputPath) -ne ".$Format") {
    throw "The output path must use the .$Format extension."
}
if ($outputPath.StartsWith($dist.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $outputPath.StartsWith($work.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The final installer must be outside dist and its intermediate directory.'
}

if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
[void](New-Item -ItemType Directory -Path $work -Force)
Write-License $work
Write-PayloadManifest $dist $work
if ($Format -eq 'msi') {
    $wixDir = Join-Path $root '.tools\wix'
    $wix = Join-Path $wixDir 'wix.exe'
    $wixVersion = $versions['WIX_VERSION']
    if (Test-Path -LiteralPath $wix) {
        $cachedVersion = & $wix --version
        if ($LASTEXITCODE -ne 0 -or $cachedVersion -notmatch ('^' + [regex]::Escape($wixVersion) + '(\+|$)')) {
            Invoke-Checked 'dotnet.exe' @('tool', 'uninstall', 'wix', '--tool-path', $wixDir)
        }
    }
    if (-not (Test-Path -LiteralPath $wix)) {
        Invoke-Checked 'dotnet.exe' @('tool', 'install', 'wix', '--tool-path', $wixDir, '--version', $wixVersion)
    }
    Build-NativeActions (Join-Path $work 'native-x64')
    $manifest = Join-Path $work 'EditorPayload.wxs'
    & (Join-Path $PSScriptRoot 'generate_installer_payload.ps1') -DistDir $dist -OutputPath $manifest -ProductVersion $productVersion
    $workPackage = Join-Path $work 'Ludork.msi'
    Push-Location $wixDir
    try {
        Invoke-Checked $wix @('extension', 'add', "WixToolset.UI.wixext/$wixVersion")
        Invoke-Checked $wix @('build', (Join-Path $PSScriptRoot 'Ludork.wxs'), $manifest,
            '-arch', 'x64', '-culture', 'en-US', '-ext', "WixToolset.UI.wixext/$wixVersion",
            '-define', "ProductVersion=$productVersion", '-define', "LicenseRtf=$work\License.en.rtf",
            '-define', "InstallerActions=$work\native-x64\bin\Release\LudorkInstallerActions.dll",
            '-bindpath', "dist=$dist", '-bindpath', "assets=$root\Editor\Assets",
            '-intermediateFolder', "$work\intermediate", '-pdbtype', 'none', '-out', $workPackage)
        Invoke-Checked $wix @('msi', 'validate', $workPackage, '-sice', 'ICE60')
    } finally { Pop-Location }
} else {
    Build-NativeActions (Join-Path $work 'native-uninstall') 'Win32'
    $compiler = Ensure-Inno
    Invoke-Checked $compiler @("/DCompilerVersion=$($versions['INNO_SETUP_VERSION'])", "/DProductVersion=$productVersion", "/DDistDir=$dist", "/DWorkDir=$work",
        "/DAssetsDir=$root\Editor\Assets", "/DUninstallActions=$work\native-uninstall\bin\Release\LudorkInstallerActions.dll",
        (Join-Path $PSScriptRoot 'Ludork.iss'))
    $workPackage = Join-Path $work "Ludork-$productVersion-windows-x64.exe"
}
Require-File $workPackage
[void](New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($outputPath)) -Force)
Copy-Item -LiteralPath $workPackage -Destination $outputPath -Force
Write-Host "Editor $($Format.ToUpperInvariant()) complete: $outputPath"
