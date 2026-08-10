param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory
)

$root = [System.IO.Path]::GetFullPath($SourceDirectory)
$files = Get-ChildItem -LiteralPath $root -Recurse -File -Filter *.py |
    Sort-Object FullName
$hash = [System.Security.Cryptography.IncrementalHash]::CreateHash(
    [System.Security.Cryptography.HashAlgorithmName]::SHA256
)
foreach ($file in $files) {
    $relative = $file.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/')
    $hash.AppendData([System.Text.Encoding]::UTF8.GetBytes($relative))
    $hash.AppendData([byte[]](0))
    $hash.AppendData([System.IO.File]::ReadAllBytes($file.FullName))
}
($hash.GetHashAndReset() | ForEach-Object { $_.ToString('x2') }) -join ''
