param(
    [Parameter(Mandatory=$true)][string]$VisualStudioHome,
    [Parameter(Mandatory=$true)][string]$JdkHome
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$cpp = Join-Path $workspace 'superresolution/native/cpp'
$cmake = Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja = Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$devCmd = Join-Path $VisualStudioHome 'Common7/Tools/VsDevCmd.bat'
# Import compiler settings into this process only; no global PATH changes.
$environmentLines = & $env:COMSPEC /d /c "call `"$devCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize x64 compiler environment' }
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
& $cmake --version
Write-Output (Get-Item (Get-Command cl.exe).Source).VersionInfo.FileVersion
$head = (& git -C (Join-Path $workspace 'superresolution') rev-parse HEAD).Trim()
& $cmake -S $cpp -B (Join-Path $cpp 'buildWindowsCoreRelease') -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release `
    "-DSR_JNI_HOME=$JdkHome" "-DSRLIB_VERSION=$head" `
    -DSR_FSR=OFF -DSR_FSR4=OFF -DSR_D3D12=OFF -DSR_XESS=OFF -DSR_NGX=OFF -DSR_STREAMLINE=OFF
if ($LASTEXITCODE -ne 0) { throw 'Native core CMake configuration failed' }
& $cmake --build (Join-Path $cpp 'buildWindowsCoreRelease') --config Release --target SR_MAIN_LIB --parallel 2 --verbose
if ($LASTEXITCODE -ne 0) { throw 'Native core build failed' }
