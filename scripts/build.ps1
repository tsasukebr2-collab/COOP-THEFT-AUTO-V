# GTAV-Coop build script.
# Requires: Visual Studio 2022 (C++ workload) or Build Tools; CMake 3.20+.
param(
    [switch]$Test  # run unit tests after build
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "build"

cmake -S $root -B $build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { Write-Error "configure failed" }

cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { Write-Error "build failed" }

Write-Output ""
Write-Output "Artifacts:"
Write-Output "  server:  $build\server\Release\coop_server.exe"
Write-Output "  client:  $build\client\Release\GTAVCoop.asi"
Write-Output "  tests:   $build\tests\Release\coop_tests.exe"

if ($Test) {
    Write-Output ""
    & "$build\tests\Release\coop_tests.exe"
    exit $LASTEXITCODE
}
