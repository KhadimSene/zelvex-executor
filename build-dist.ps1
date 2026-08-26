param(
    [switch]$SkipInnoSetup
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$DistDir = Join-Path $BuildDir "dist"
$QtBin = "C:\msys64\ucrt64\bin"

Write-Host "=== Configuring CMake (Release) ===" -ForegroundColor Cyan
Push-Location $ProjectRoot
try {
    cmake -G "Ninja" -B build -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

    Write-Host "=== Building Zelvex (Release) ===" -ForegroundColor Cyan
    cmake --build build
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

    if (Test-Path $DistDir) {
        Remove-Item -Recurse -Force $DistDir
    }
    New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

    Copy-Item (Join-Path $BuildDir "Zelvex.exe") $DistDir

    Write-Host "=== Running windeployqt ===" -ForegroundColor Cyan
    try {
        $prevEAP = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & "$QtBin\windeployqt.exe" --release (Join-Path $DistDir "Zelvex.exe") 2>&1 | ForEach-Object { Write-Host $_ }
    } catch {
        Write-Host "  windeployqt warning: $_" -ForegroundColor Yellow
    } finally {
        $ErrorActionPreference = "Stop"
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  windeployqt exited with code $LASTEXITCODE (translation warning is non-fatal; Qt DLLs still deployed)" -ForegroundColor Yellow
    }

    Write-Host "=== Copying libkeystone.dll ===" -ForegroundColor Cyan
    Copy-Item (Join-Path $QtBin "libkeystone.dll") $DistDir

    Write-Host "=== Copying Lua executor DLL (lua_dll_new.dll) ===" -ForegroundColor Cyan
    $luaDll = Join-Path $BuildDir "bin\liblua_dll.dll"
    if (-not (Test-Path $luaDll)) { $luaDll = Join-Path $BuildDir "lua_dll.dll" }
    if (Test-Path $luaDll) {
        Copy-Item $luaDll (Join-Path $DistDir "lua_dll_new.dll") -Force
        Copy-Item $luaDll (Join-Path $DistDir "lua_dll.dll") -Force
        Write-Host "  copied $luaDll -> lua_dll_new.dll / lua_dll.dll"
    } else {
        Write-Host "  WARNING: lua_dll.dll not found; installer will be missing the executor" -ForegroundColor Yellow
    }

    # Bundle all transitive MinGW/MSYS2 native dependencies (libfreetype-6.dll,
    # libharfbuzz, libgcc_s_seh-1.dll for the Qt DLLs, etc.) that windeployqt misses.
    Write-Host "=== Bundling transitive MinGW DLLs ===" -ForegroundColor Cyan
    & "$ProjectRoot\copy-runtime-deps.ps1" -DistDir $DistDir
    if ($LASTEXITCODE -ne 0) { throw "copy-runtime-deps failed" }

    if (-not $SkipInnoSetup) {
        $iscc = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
        if (-not $iscc) {
            $isccPath = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
            if (Test-Path $isccPath) {
                $iscc = $isccPath
            }
        }
        if ($iscc) {
            Write-Host "=== Building Inno Setup installer ===" -ForegroundColor Cyan
            $issFile = Join-Path $ProjectRoot "installer.iss"
            if ($iscc -is [System.Management.Automation.CommandInfo]) {
                & $iscc.Source $issFile
            } else {
                & $iscc $issFile
            }
            if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed" }
            $installerDir = Join-Path $BuildDir "installer"
            if (Test-Path $installerDir) {
                Get-ChildItem $installerDir -Filter "*.exe" | Select-Object Name | Format-Table -AutoSize
            }
        } else {
            Write-Host "  iscc not found - install Inno Setup to build installer" -ForegroundColor Yellow
        }
    }

    Write-Host "=== Distribution folder: $DistDir ===" -ForegroundColor Green
    Get-ChildItem $DistDir | Select-Object Name | Format-Table -AutoSize

} finally {
    Pop-Location
}
