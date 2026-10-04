param([string]$VisualStudioHome='D:\Programs File2\Microsoft Visual Studio\18\Community',
      [string]$JdkHome='D:\Programs File2\Eclipse Adoptium\jdk-21.0.12.101-hotspot')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskDevCmd=Join-Path $VisualStudioHome 'Common7/Tools/VsDevCmd.bat'
$taskEnvironment=& $env:COMSPEC /d /c "call `"$taskDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if($LASTEXITCODE -ne 0){throw 'Compiler initialization failed'}
foreach($taskLine in $taskEnvironment){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
$taskCmake=Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$taskNinja=Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$taskJdkCmake=$JdkHome.Replace('\','/')
$env:JAVA_HOME=$taskJdkCmake
& $taskCmake -S "$taskRoot/wisteria/native/dlssg" -B "$taskRoot/wisteria/native/build/dlssg-windows-x64" -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" -DCMAKE_BUILD_TYPE=Release "-DJAVA_HOME=$taskJdkCmake"
if($LASTEXITCODE -ne 0){throw 'DLSS-G bridge configuration failed'}
& $taskCmake --build "$taskRoot/wisteria/native/build/dlssg-windows-x64" --target wisteria_dlssg_bridge --parallel 2
if($LASTEXITCODE -ne 0){throw 'DLSS-G bridge build failed'}
