$ErrorActionPreference = 'Stop'
$combinedRoot = Split-Path $PSScriptRoot -Parent
$combinedVs = 'D:\Programs File2\Microsoft Visual Studio\18\Community'
$combinedDevCmd = Join-Path $combinedVs 'Common7/Tools/VsDevCmd.bat'
$combinedEnvironment = & $env:COMSPEC /d /c "call `"$combinedDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize compiler' }
foreach ($combinedLine in $combinedEnvironment) {
 if ($combinedLine -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
$combinedCmake = Join-Path $combinedVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$combinedNinja = Join-Path $combinedVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$combinedSource = Join-Path $combinedRoot 'experiments/dlssg-vk-d3d12-x2'
& $combinedCmake -S $combinedSource -B "$combinedSource/build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$combinedNinja" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Combined harness configure failed' }
& $combinedCmake --build "$combinedSource/build" --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Combined harness build failed' }
& "$combinedSource/build/dlssg_vk_d3d12_x2.exe" --self-test "$combinedSource/build/cpu-tests"
if ($LASTEXITCODE -ne 0) { throw 'CPU fixture/camera/reducer/memory intersection/checkpoint tests failed' }
& 'D:/Python311/python.exe' "$PSScriptRoot/summarize_dlssg_vk_d3d12_x2.py" --self-test
if ($LASTEXITCODE -ne 0) { throw 'CPU combined evidence gate tests failed' }
