$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path $PSScriptRoot -Parent
$probeVs = 'D:\Programs File2\Microsoft Visual Studio\18\Community'
$probeDevCmd = Join-Path $probeVs 'Common7/Tools/VsDevCmd.bat'
$probeEnvironment = & $env:COMSPEC /d /c "call `"$probeDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize compiler' }
foreach ($probeLine in $probeEnvironment) {
    if ($probeLine -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
$probeCmake = Join-Path $probeVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$probeNinja = Join-Path $probeVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$probeSource = Join-Path $probeRoot 'experiments/ngx-dlssg-probe'
& $probeCmake -S $probeSource -B "$probeSource/build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$probeNinja" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Probe configure failed' }
& $probeCmake --build "$probeSource/build" --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Probe build failed' }
