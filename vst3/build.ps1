# Build the desktop VST3/standalone on Windows.
# Requires CMake and Visual Studio 2022 (Desktop C++).

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $PSScriptRoot "build" }

$cmakeArgs = @("-S", (Join-Path $Root "vst3"), "-B", $Build, "-DCMAKE_BUILD_TYPE=Release")
if ($env:JUCE_DIR) { $cmakeArgs += "-DJUCE_DIR=$env:JUCE_DIR" }

cmake @cmakeArgs
cmake --build $Build --config Release --parallel

Write-Host ""
Write-Host "VST3 bundle: $Build\Polivoks1982_artefacts\Release\VST3\Polivoks 1982.vst3"
