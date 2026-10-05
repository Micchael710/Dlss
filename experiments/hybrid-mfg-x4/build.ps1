$ErrorActionPreference='Stop'
$taskRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$taskEvidence=Join-Path $taskRoot 'logs/research/hybrid-mfg-x4-generation'
$taskCounter=Join-Path $PSScriptRoot 'build/build-count.txt'
New-Item -ItemType Directory -Path "$PSScriptRoot/build" -Force | Out-Null
$taskCount=if(Test-Path $taskCounter){[int](Get-Content $taskCounter)}else{0}
if($taskCount -ge 2){throw 'Two builds already used; STOP'}
($taskCount+1) | Set-Content $taskCounter
$taskVs='D:\Programs File2\Microsoft Visual Studio\18\Community'
$taskDev=Join-Path $taskVs 'Common7/Tools/VsDevCmd.bat'
$taskEnvironment=& $env:COMSPEC /d /c "call `"$taskDev`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if($LASTEXITCODE -ne 0){throw 'Compiler environment failed'}
foreach($line in $taskEnvironment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
$taskCmake=Join-Path $taskVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$taskNinja=Join-Path $taskVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
"BUILD_COUNT=$($taskCount+1)" | Add-Content "$taskEvidence/build.log"
& $taskCmake -S $PSScriptRoot -B "$PSScriptRoot/build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" -DCMAKE_BUILD_TYPE=Release 2>&1 | Tee-Object -FilePath "$taskEvidence/build.log" -Append
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
& $taskCmake --build "$PSScriptRoot/build" --parallel 2 2>&1 | Tee-Object -FilePath "$taskEvidence/build.log" -Append
if($LASTEXITCODE -ne 0){throw 'C++ build failed'}
$taskFxc=Join-Path $env:WindowsSdkVerBinPath 'x64/fxc.exe'
foreach($entry in @('FixtureCS','HybridInterpolationCS','ReductionCS')){
 & $taskFxc /nologo /WX /T cs_5_0 /E $entry /Fo "$PSScriptRoot/build/$entry.cso" "$PSScriptRoot/HybridInterpolationCS.hlsl" 2>&1 | Tee-Object -FilePath "$taskEvidence/build.log" -Append
 if($LASTEXITCODE -ne 0){throw "Shader build failed: $entry"}
}
