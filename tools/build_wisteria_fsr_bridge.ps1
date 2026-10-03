param(
    [string]$VisualStudioHome = 'D:\Programs File2\Microsoft Visual Studio\18\Community',
    [string]$JdkHome = 'D:\Programs File2\Eclipse Adoptium\jdk-21.0.12.101-hotspot'
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskDevCmd = Join-Path $VisualStudioHome 'Common7/Tools/VsDevCmd.bat'
$taskEnvironment = & $env:COMSPEC /d /c "call `"$taskDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize x64 compiler' }
foreach ($taskLine in $taskEnvironment) {
    if ($taskLine -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
}
$taskCmake = Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$taskNinja = Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$taskSource = Join-Path $taskRoot 'wisteria/native/fsr'
$taskBuild = Join-Path $taskRoot 'wisteria/native/build/fsr-windows-x64'
& $taskCmake -S $taskSource -B $taskBuild -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" -DCMAKE_BUILD_TYPE=Release `
    "-DFSR_SDK_ROOT=$taskRoot/third_party/amd-fidelityfx-1.1.4" "-DSR_VULKAN_HEADERS=$taskRoot/superresolution/native/cpp/third_party" "-DJNI_HOME=$JdkHome"
if ($LASTEXITCODE -ne 0) { throw 'FSR bridge configuration failed' }
& $taskCmake --build $taskBuild --target wisteria_fsr_bridge --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'FSR bridge build failed' }
