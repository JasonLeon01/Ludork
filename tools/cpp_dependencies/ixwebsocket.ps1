param([Parameter(Mandatory = $true)][string]$CppFolder)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$cppRoot = (Resolve-Path -LiteralPath $CppFolder).Path
if (-not (Test-Path -LiteralPath (Join-Path $cppRoot 'CMakeLists.txt') -PathType Leaf)) {
    throw "CMakeLists.txt was not found: $cppRoot"
}

$versions = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $projectRoot 'versions.conf')) {
    if ($line -match '^(IXWEBSOCKET_VERSION|IXWEBSOCKET_COMMIT)=(\S+)$') {
        $versions[$Matches[1]] = $Matches[2]
    }
}
$version = $versions['IXWEBSOCKET_VERSION']
$commit = $versions['IXWEBSOCKET_COMMIT']
if ($version -notmatch '^\d+\.\d+\.\d+$' -or $commit -notmatch '^[0-9a-f]{40}$') {
    throw 'A valid IXWEBSOCKET_VERSION and IXWEBSOCKET_COMMIT are required in versions.conf.'
}

$dependencyRoot = [IO.Path]::GetFullPath((Join-Path $cppRoot 'Engine\ThirdParty'))
$dependencyDirectory = [IO.Path]::GetFullPath((Join-Path $dependencyRoot 'IXWebSocket'))
$marker = "$version-$commit"
$markerPath = Join-Path $dependencyDirectory '.ludork-version'
if ((Test-Path -LiteralPath $markerPath -PathType Leaf) -and
    (Get-Content -LiteralPath $markerPath -Raw).Trim() -eq $marker -and
    (Test-Path -LiteralPath (Join-Path $dependencyDirectory 'CMakeLists.txt') -PathType Leaf)) {
    Write-Output "Using existing IXWebSocket $version."
    exit 0
}

$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$workDirectory = [IO.Path]::GetFullPath((Join-Path $temporaryRoot ('ludork-ixwebsocket-' + [guid]::NewGuid().ToString('N'))))
if (-not $workDirectory.StartsWith($temporaryRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
    -not $dependencyDirectory.StartsWith($dependencyRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'IXWebSocket dependency paths escaped their owning directories.'
}

New-Item -ItemType Directory -Path $workDirectory | Out-Null
try {
    Write-Output "Downloading IXWebSocket $version ($commit)..."
    $archivePath = Join-Path $workDirectory 'ixwebsocket.tar.gz'
    & curl.exe --location --fail --show-error --retry 3 --retry-connrefused --retry-delay 5 --retry-max-time 900 --connect-timeout 15 --max-time 300 --output $archivePath "https://github.com/machinezone/IXWebSocket/archive/$commit.tar.gz"
    if ($LASTEXITCODE -ne 0) {
        throw "IXWebSocket download failed with exit code $LASTEXITCODE."
    }
    & tar.exe -xzf $archivePath -C $workDirectory
    if ($LASTEXITCODE -ne 0) {
        throw "IXWebSocket extraction failed with exit code $LASTEXITCODE."
    }
    $sourceDirectory = [IO.Path]::GetFullPath((Join-Path $workDirectory "IXWebSocket-$commit"))
    if (-not $sourceDirectory.StartsWith($workDirectory + '\', [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath (Join-Path $sourceDirectory 'CMakeLists.txt') -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path $sourceDirectory 'ixwebsocket\IXWebSocket.h') -PathType Leaf)) {
        throw 'IXWebSocket source folder was not found after extraction.'
    }
    New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
    if (Test-Path -LiteralPath $dependencyDirectory) {
        Remove-Item -LiteralPath $dependencyDirectory -Recurse -Force
    }
    Move-Item -LiteralPath $sourceDirectory -Destination $dependencyDirectory
    Set-Content -LiteralPath $markerPath -Value $marker -Encoding Ascii
} finally {
    Remove-Item -LiteralPath $workDirectory -Recurse -Force
}
