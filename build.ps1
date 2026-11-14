# Build the MPC ARM plugin and the desktop VST2 from a pinned MPC Plugin SDK.
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$SdkRepo = if ($env:SDK_REPO) { $env:SDK_REPO } else { "https://github.com/inmusic-software/mpc-plugin-builder-sdk.git" }
$SdkCommit = if ($env:SDK_COMMIT) { $env:SDK_COMMIT } else { "febd2ca5bd9eb16c4a412837196e145566bc981b" }
$RepoUrl = if ($env:REPO_URL) { $env:REPO_URL } else { "https://github.com/tkleisas/polivoks-1982-vst" }
$Python = if ($env:PYTHON) { $env:PYTHON } else { "python" }
$SdkDir = if ($env:SDK_DIR) { $env:SDK_DIR } else { Join-Path $Root ".sdk" }

function Invoke-Checked {
    param([string]$Exe, [string[]]$Arguments)
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

if (-not (Test-Path (Join-Path $SdkDir ".git"))) {
    Write-Host "-- cloning MPC Plugin SDK"
    Invoke-Checked "git" @("clone", $SdkRepo, $SdkDir)
}
& git -C $SdkDir fetch --quiet origin $SdkCommit
& git -C $SdkDir checkout --quiet $SdkCommit
if ($LASTEXITCODE -ne 0) { throw "could not check out SDK commit $SdkCommit" }

$PluginDir = Join-Path $SdkDir "plugins/Polivoks 1982"
if (Test-Path $PluginDir) { Remove-Item -Recurse -Force $PluginDir }
New-Item -ItemType Directory -Force -Path (Join-Path $SdkDir "plugins") | Out-Null
Copy-Item -Recurse (Join-Path $Root "plugin/Polivoks 1982") (Join-Path $SdkDir "plugins")

$ZigArgs = @()
if ($env:ZIG) { $ZigArgs = @("--zig", $env:ZIG) }
$BuildScript = Join-Path $SdkDir "tools/build.py"

Write-Host "-- building MPC (native + ARM32 + ARM64)"
Invoke-Checked $Python (@($BuildScript, $PluginDir, "--target", "all") + $ZigArgs)

Write-Host "-- building desktop VST2"
Invoke-Checked $Python (@($BuildScript, $PluginDir, "--target", "desktop") + $ZigArgs)

Write-Host "-- running host simulator test"
$HostSim = Join-Path $SdkDir ".sdk-tools/host_sim.exe"
$Native = Join-Path $PluginDir "build/native/polivoks-1982.dll"
if ((Test-Path $HostSim) -and (Test-Path $Native)) {
    $TestWav = Join-Path $Root "dist/.host-sim-test.wav"
    New-Item -ItemType Directory -Force -Path (Join-Path $Root "dist") | Out-Null
    Invoke-Checked $HostSim @($Native, "test", "--seconds", "3", "--notes", "48,60,67", "--wav", $TestWav)
    Remove-Item -Force $TestWav -ErrorAction SilentlyContinue
} else {
    Write-Host "note: host_sim or native DLL not found; skipping the simulator test"
}

Write-Host "-- packaging release assets"
Invoke-Checked $Python @(
    (Join-Path $Root "tools/make_release.py"),
    "--sdk", $SdkDir,
    "--out", (Join-Path $Root "dist"),
    "--repo", $RepoUrl,
    "--desktop-name", "windows-vst2"
)

Write-Host ""
Write-Host "Done. Release assets:"
Get-ChildItem (Join-Path $Root "dist") -Filter "*.zip" | Format-Table Name, Length
