param(
    [Parameter(Mandatory = $true)]
    [string]$DistDir,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [Parameter(Mandatory = $true)]
    [string]$ProductVersion
)

function Get-StableHash([string]$value)
{
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try
    {
        $hash = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($value))
    }
    finally
    {
        $sha256.Dispose()
    }
    return $hash
}

function Get-Hex([byte[]]$hash)
{
    return -join ($hash | ForEach-Object { $_.ToString('x2') })
}

function Get-StableGuid([string]$value)
{
    [byte[]]$hash = Get-StableHash $value
    [byte[]]$guidBytes = New-Object byte[] 16
    [Array]::Copy($hash, $guidBytes, 16)
    $guidBytes[6] = [byte](($guidBytes[6] -band 0x0f) -bor 0x50)
    $guidBytes[8] = [byte](($guidBytes[8] -band 0x3f) -bor 0x80)
    return (New-Object Guid (,$guidBytes)).ToString().ToUpperInvariant()
}

$resolvedDist = (Resolve-Path -LiteralPath $DistDir).Path.TrimEnd('\')
$directories = @(
    Get-Item -LiteralPath $resolvedDist
    Get-ChildItem -LiteralPath $resolvedDist -Directory -Recurse -Force |
        Sort-Object FullName
)

$directoryIds = @{}
$directoryChildren = @{}
foreach ($directory in $directories)
{
    $relativeDirectory = $directory.FullName.Substring($resolvedDist.Length).TrimStart('\')
    if ($relativeDirectory.Length -eq 0)
    {
        $directoryIds[$relativeDirectory] = 'INSTALLFOLDER'
        continue
    }

    $normalizedDirectory = $relativeDirectory.ToLowerInvariant()
    [byte[]]$directoryHash = Get-StableHash "Ludork.PayloadDirectory`0$normalizedDirectory"
    $directoryIds[$relativeDirectory] = "PayloadDir_$((Get-Hex $directoryHash).Substring(0, 24))"
    $parentDirectory = Split-Path -Path $relativeDirectory -Parent
    if ($null -eq $parentDirectory)
    {
        $parentDirectory = ''
    }
    if (-not $directoryChildren.ContainsKey($parentDirectory))
    {
        $directoryChildren[$parentDirectory] = [System.Collections.Generic.List[string]]::new()
    }
    $directoryChildren[$parentDirectory].Add($relativeDirectory)
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('<?xml version="1.0" encoding="utf-8"?>')
$lines.Add('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
$lines.Add('  <Fragment>')
$lines.Add('    <DirectoryRef Id="INSTALLFOLDER">')

function Add-DirectoryChildren([string]$parentDirectory, [int]$indent)
{
    if (-not $directoryChildren.ContainsKey($parentDirectory))
    {
        return
    }

    foreach ($relativeDirectory in @($directoryChildren[$parentDirectory] | Sort-Object))
    {
        $directoryName = Split-Path -Path $relativeDirectory -Leaf
        $escapedName = [System.Security.SecurityElement]::Escape($directoryName)
        $directoryId = $directoryIds[$relativeDirectory]
        $padding = ' ' * $indent
        $lines.Add("$padding<Directory Id=`"$directoryId`" Name=`"$escapedName`">")
        Add-DirectoryChildren $relativeDirectory ($indent + 2)
        $lines.Add("$padding</Directory>")
    }
}

Add-DirectoryChildren '' 6
$lines.Add('    </DirectoryRef>')
$lines.Add('  </Fragment>')
$lines.Add('  <Fragment>')
$lines.Add('    <ComponentGroup Id="EditorFiles">')

foreach ($directory in $directories)
{
    $relativeDirectory = $directory.FullName.Substring($resolvedDist.Length).TrimStart('\')
    $normalizedDirectory = $relativeDirectory.ToLowerInvariant()
    [byte[]]$componentHash = Get-StableHash "Ludork.PayloadComponent`0$normalizedDirectory"
    $componentSuffix = (Get-Hex $componentHash).Substring(0, 24)
    $componentId = "Payload_$componentSuffix"
    $componentGuid = Get-StableGuid "Ludork.PayloadComponent`0$ProductVersion`0$normalizedDirectory"
    $directoryId = $directoryIds[$relativeDirectory]
    $files = @(
        Get-ChildItem -LiteralPath $directory.FullName -File -Force |
            Where-Object {
                $relativeDirectory.Length -ne 0 -or
                -not $_.Name.Equals('Ludork.exe', [System.StringComparison]::OrdinalIgnoreCase)
            } |
            Sort-Object Name
    )

    $lines.Add("      <Component Id=`"$componentId`" Directory=`"$directoryId`" Guid=`"$componentGuid`">")

    $lines.Add("        <RegistryValue Root=`"HKCU`" Key=`"Software\Ludork\Installer\Components`" Name=`"$componentId`" Type=`"integer`" Value=`"1`" KeyPath=`"yes`" />")
    $lines.Add("        <RemoveFolder Id=`"Remove_$componentSuffix`" On=`"uninstall`" />")

    if ($files.Count -eq 0)
    {
        $lines.Add('        <CreateFolder />')
    }
    else
    {
        foreach ($file in $files)
        {
            $relativeFile = $file.FullName.Substring($resolvedDist.Length).TrimStart('\')
            $normalizedFile = $relativeFile.ToLowerInvariant()
            [byte[]]$fileHash = Get-StableHash "Ludork.PayloadFile`0$normalizedFile"
            $fileSuffix = (Get-Hex $fileHash).Substring(0, 24)
            $escapedFile = [System.Security.SecurityElement]::Escape($relativeFile)
            $lines.Add("        <File Id=`"PayloadFile_$fileSuffix`" Source=`"!(bindpath.dist)\$escapedFile`" />")
        }
    }

    $lines.Add('      </Component>')
}

$lines.Add('    </ComponentGroup>')
$lines.Add('  </Fragment>')
$lines.Add('</Wix>')
$lines | Set-Content -LiteralPath $OutputPath -Encoding UTF8
