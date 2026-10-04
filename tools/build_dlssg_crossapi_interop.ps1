param([string]$Target = '')
$ErrorActionPreference = 'Stop'
$harnessRoot = Split-Path $PSScriptRoot -Parent
$harnessVs = 'D:\Programs File2\Microsoft Visual Studio\18\Community'
$harnessDevCmd = Join-Path $harnessVs 'Common7/Tools/VsDevCmd.bat'
$harnessEnvironment = & $env:COMSPEC /d /c "call `"$harnessDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize compiler' }
foreach ($harnessLine in $harnessEnvironment) {
 if ($harnessLine -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
$harnessCmake = Join-Path $harnessVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$harnessNinja = Join-Path $harnessVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$harnessSource = Join-Path $harnessRoot 'experiments/dlssg-crossapi-interop'
& $harnessCmake -S $harnessSource -B "$harnessSource/build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$harnessNinja" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Harness configure failed' }
if ($Target) { & $harnessCmake --build "$harnessSource/build" --parallel 2 --target $Target }
else { & $harnessCmake --build "$harnessSource/build" --parallel 2 }
if ($LASTEXITCODE -ne 0) { throw 'Harness build failed' }
