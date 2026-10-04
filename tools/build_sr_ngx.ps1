param([string]$VisualStudioHome='D:\Programs File2\Microsoft Visual Studio\18\Community',
      [string]$JdkHome='C:\Program Files\Java\jdk-25.0.4')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskDevCmd=Join-Path $VisualStudioHome 'Common7/Tools/VsDevCmd.bat'
$taskEnvironment=& $env:COMSPEC /d /c "call `"$taskDevCmd`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if($LASTEXITCODE -ne 0){throw 'Compiler initialization failed'}
foreach($taskLine in $taskEnvironment){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
$env:JAVA_HOME=$JdkHome
$taskCmake=Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$taskNinja=Join-Path $VisualStudioHome 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
& $taskCmake -S "$taskRoot/superresolution/native/cpp/SRNativeNGX" -B "$taskRoot/builds/experimental/dlss-sr/native" -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_FLAGS_DEBUG=/Od /Z7" "-DCMAKE_CXX_FLAGS_DEBUG=/Od /Z7" "-DDLSS_SDK_DIR=$taskRoot/logs/research/dlssg-ampere/deep/official-sdk" "-DSR_NGX_LIBRARY=$taskRoot/experiments/ngx-dlssg-probe/vendor/nvsdk_ngx_s.lib"
if($LASTEXITCODE -ne 0){throw 'NGX configuration failed'}
& $taskCmake --build "$taskRoot/builds/experimental/dlss-sr/native" --target SR_NGX_LIB --parallel 2
if($LASTEXITCODE -ne 0){throw 'NGX binding build failed'}
