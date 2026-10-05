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
"BUILD_COUNT=$($taskCount+1)" | Add-Content "$taskEvidence/build.log"
$taskCl=(Get-Command cl.exe -ErrorAction Stop).Source
$taskLink=(Get-Command link.exe -ErrorAction Stop).Source
$taskExpected=Join-Path $taskVs 'VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64'
if($taskCl -ine (Join-Path $taskExpected 'cl.exe') -or $taskLink -ine (Join-Path $taskExpected 'link.exe')){throw 'Unexpected compiler/linker; STOP'}
$taskLog=@("DIRECT_BUILD_CL_PATH=$taskCl","DIRECT_BUILD_LINK_PATH=$taskLink","CL_ENV_BEFORE=$env:CL","_CL__ENV_BEFORE=$env:_CL_","LINK_ENV_BEFORE=$env:LINK","_LINK__ENV_BEFORE=$env:_LINK_")
$taskLog | Add-Content "$taskEvidence/build.log"
$taskLog | Set-Content "$taskEvidence/direct-build.txt"
# Neutralize flags only in this build process. No machine/user environment edits.
$env:CL='';$env:_CL_='';$env:LINK='';$env:_LINK_=''
'CL_ENV_NEUTRALIZED=YES' | Add-Content "$taskEvidence/build.log","$taskEvidence/direct-build.txt"
$taskExe=Join-Path $PSScriptRoot 'build/hybrid_mfg_x4.exe'
$taskObject=Join-Path $PSScriptRoot 'build/hybrid_mfg_x4.obj'
$taskInclude=Join-Path $taskRoot 'logs/research/dlssg-ampere/deep/official-sdk/include'
$taskNgx=Join-Path $taskRoot 'experiments/ngx-dlssg-probe/vendor/nvsdk_ngx_s.lib'
$taskArgs=@('/nologo','/std:c++20','/EHsc','/O2','/MT','/DNDEBUG','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/utf-8','/W4',"/I$taskInclude","/Fo$taskObject","/Fe$taskExe",(Join-Path $PSScriptRoot 'main.cpp'),'/link','/INCREMENTAL:NO','/OPT:REF','/OPT:ICF',$taskNgx,'advapi32.lib','user32.lib','shell32.lib','ole32.lib','dxgi.lib','d3d12.lib','d3dcompiler.lib','version.lib','bcrypt.lib','wintrust.lib','psapi.lib','shlwapi.lib')
('DIRECT_BUILD_ARGUMENTS='+($taskArgs -join ' | ')) | Add-Content "$taskEvidence/direct-build.txt"
& $taskCl @taskArgs 2>&1 | Tee-Object -FilePath "$taskEvidence/build.log" -Append
$taskClExit=$LASTEXITCODE
"CL_EXIT_CODE=$taskClExit" | Add-Content "$taskEvidence/direct-build.txt"
"CPP_EXE_CREATED=$(if(Test-Path $taskExe){'YES'}else{'NO'})" | Add-Content "$taskEvidence/direct-build.txt"
"PDB_FILES_CREATED=$(@(Get-ChildItem "$PSScriptRoot/build" -Filter '*.pdb' -File).Count)" | Add-Content "$taskEvidence/direct-build.txt"
if($taskClExit -ne 0){throw 'Direct C++ compile/link failed; no third build'}
if(!(Test-Path $taskExe)){throw 'Executable missing'}
'DIRECT_CPP_BUILD=PASS','DIRECT_LINK=PASS','C1902_BYPASS_RESULT=PASS' | Add-Content "$taskEvidence/direct-build.txt"
$taskFxc=Join-Path $env:WindowsSdkVerBinPath 'x64/fxc.exe'
foreach($entry in @('FixtureCS','HybridInterpolationCS','ReductionCS')){
 & $taskFxc /nologo /WX /T cs_5_0 /E $entry /Fo "$PSScriptRoot/build/$entry.cso" "$PSScriptRoot/HybridInterpolationCS.hlsl" 2>&1 | Tee-Object -FilePath "$taskEvidence/build.log" -Append
 if($LASTEXITCODE -ne 0){throw "Shader build failed: $entry"}
 "${entry}_BUILD=PASS" | Add-Content "$taskEvidence/direct-build.txt"
}
