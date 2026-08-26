param(
    [Parameter(Mandatory=$true)]
    [string]$DistDir,
    [string]$MingwBin = "C:\msys64\ucrt64\bin"
)

$objdump = "$MingwBin\objdump.exe"
if (-not (Test-Path $objdump)) {
    Write-Error "objdump not found at $objdump"
    exit 1
}
if (-not (Test-Path $DistDir)) {
    Write-Error "Dist dir not found: $DistDir"
    exit 1
}

# Queue of DLLs to process (relative paths from DistDir)
$toProcess = New-Object System.Collections.Queue
# Set of DLLs already processed or known (basename, lower)
$processed = @{}

# Seed with all dlls currently present in DistDir (recursive)
Get-ChildItem -Path $DistDir -Filter *.dll -Recurse | ForEach-Object {
    $rel = $_.FullName.Substring($DistDir.TrimEnd('\').Length + 1)
    $toProcess.Enqueue($rel)
    $processed[$_.Name.ToLower()] = $true
}

$copied = 0
while ($toProcess.Count -gt 0) {
    $rel = $toProcess.Dequeue()
    $full = Join-Path $DistDir $rel
    # Get dependencies via objdump
    $deps = & $objdump -p $full 2>$null | Where-Object { $_ -match '^\s*DLL Name:\s*(.+)$' } | ForEach-Object { $matches[1].Trim() }
    foreach ($dep in $deps) {
        $depLower = $dep.ToLower()
        # Skip already-known
        if ($processed.ContainsKey($depLower)) { continue }
        $processed[$depLower] = $true
        # Only bundle DLLs that exist in the MinGW bin dir (native deps)
        $src = Join-Path $MingwBin $dep
        if (Test-Path $src) {
            Copy-Item $src $DistDir
            $copied++
            Write-Host "  Copied: $dep" -ForegroundColor Green
            # New DLL to process
            $toProcess.Enqueue($dep)
        }
    }
}

Write-Host "Bundled $copied transitive MinGW DLL(s) into $DistDir" -ForegroundColor Cyan
