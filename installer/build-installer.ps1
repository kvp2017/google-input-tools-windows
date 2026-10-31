$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$iss = Join-Path $PSScriptRoot "TamilIme.iss"
$artifacts = Join-Path $root "artifacts"

New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
cmake -S $root -B $build -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

cmake --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

ctest --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed." }

$iscc = (Get-Command "ISCC.exe" -ErrorAction SilentlyContinue)?.Source
if (-not $iscc) {
    $candidates = @(
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { $iscc = $candidate; break }
    }
}
if (-not $iscc) {
    throw "Inno Setup 6 compiler not found. Install Inno Setup 6 and rerun this script."
}

& $iscc $iss
if ($LASTEXITCODE -ne 0) { throw "Inno Setup packaging failed." }
Write-Host "Installer created in $artifacts"
