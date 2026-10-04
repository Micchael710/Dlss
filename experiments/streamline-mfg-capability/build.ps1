param([string]$VisualStudioHome='D:\Programs File2\Microsoft Visual Studio\18\Community')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$taskDevCmd=Join-Path $VisualStudioHome 'Common7/Tools/VsDevCmd.bat'
$taskEnvironment=& $env:COMSPEC /d /c "call `"$taskDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if($LASTEXITCODE -ne 0){throw 'Compiler initialization failed'}
foreach($taskLine in $taskEnvironment){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
$taskCmake=Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$taskNinja=Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$taskBuild=Join-Path $taskRoot 'builds/experimental/streamline-mfg-capability'
& $taskCmake -S $PSScriptRoot -B $taskBuild -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" -DCMAKE_BUILD_TYPE=Release
if($LASTEXITCODE -ne 0){throw 'Harness configuration failed'}
& $taskCmake --build $taskBuild --parallel 2
if($LASTEXITCODE -ne 0){throw 'Harness build failed'}
