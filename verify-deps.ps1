$DistDir = "C:\Users\Admin\Documents\Zelvex\build\dist"
$objdump = "C:\msys64\ucrt64\bin\objdump.exe"
$sys32 = "C:\Windows\System32"

$distDlls = @{}
Get-ChildItem -Path $DistDir -Filter *.dll -Recurse | ForEach-Object { $distDlls[$_.Name.ToLower()] = $true }

$sysDlls = @{}
Get-ChildItem -Path $sys32 -Filter *.dll | ForEach-Object { $sysDlls[$_.Name.ToLower()] = $true }

$missing = @{}
Get-ChildItem -Path $DistDir -Filter *.dll -Recurse | ForEach-Object {
    $out = & $objdump -p $_.FullName 2>$null
    foreach ($line in $out) {
        if ($line -match "DLL Name:\s*(.+)") {
            $d = $matches[1].Trim().ToLower()
            if (-not $distDlls.ContainsKey($d) -and -not $sysDlls.ContainsKey($d)) {
                $missing[$d] = $true
            }
        }
    }
}

if ($missing.Count -eq 0) {
    Write-Host "ALL DEPENDENCIES SATISFIED - no missing DLLs" -ForegroundColor Green
} else {
    Write-Host "MISSING DLLs:" -ForegroundColor Red
    $missing.Keys | Sort-Object | ForEach-Object { Write-Host "  $_" }
}
