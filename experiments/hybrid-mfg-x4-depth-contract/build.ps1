$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$taskEvidence = Join-Path $taskRoot 'logs/research/hybrid-mfg-x4-depth-contract'
$taskCounter = Join-Path $PSScriptRoot 'build/build-count.txt'
New-Item -ItemType Directory -Path "$PSScriptRoot/build" -Force | Out-Null
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null

$taskCount = if (Test-Path $taskCounter) { [int](Get-Content $taskCounter) } else { 0 }
if ($taskCount -ge 1) { throw 'BUILD_COUNT_THIS_ITERATION limit reached (1); STOP' }
($taskCount + 1) | Set-Content $taskCounter

$taskVs = 'D:\Programs File2\Microsoft Visual Studio\18\Community'
$taskDev = Join-Path $taskVs 'Common7/Tools/VsDevCmd.bat'
$taskEnvironment = & $env:COMSPEC /d /c "call `"$taskDev`" -no_logo -arch=amd64 -host_arch=amd64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment failed' }
foreach ($line in $taskEnvironment) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
"BUILD_COUNT_THIS_ITERATION=$($taskCount + 1)" | Add-Content "$taskEvidence/build.log"

$taskCl = (Get-Command cl.exe -ErrorAction Stop).Source
$taskLink = (Get-Command link.exe -ErrorAction Stop).Source
$taskExpected = Join-Path $taskVs 'VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64'
if ($taskCl -ine (Join-Path $taskExpected 'cl.exe') -or $taskLink -ine (Join-Path $taskExpected 'link.exe')) {
    throw 'Unexpected compiler/linker; STOP'
}

$taskLog = @("DIRECT_BUILD_CL_PATH=$taskCl", "DIRECT_BUILD_LINK_PATH=$taskLink", "CL_ENV_BEFORE=$env:CL", "_CL__ENV_BEFORE=$env:_CL_", "LINK_ENV_BEFORE=$env:LINK", "_LINK__ENV_BEFORE=$env:_LINK_")
$taskLog | Add-Content "$taskEvidence/build.log"

# Neutralize flags only in this build process
$env:CL = ''; $env:_CL_ = ''; $env:LINK = ''; $env:_LINK_ = ''
'CL_ENV_NEUTRALIZED=YES' | Add-Content "$taskEvidence/build.log"

$taskExe = Join-Path $PSScriptRoot 'build/hybrid_mfg_x4_depth_contract.exe'
$taskObject = Join-Path $PSScriptRoot 'build/hybrid_mfg_x4_depth_contract.obj'
$taskInclude = Join-Path $taskRoot 'logs/research/dlssg-ampere/deep/official-sdk/include'
$taskNgx = Join-Path $taskRoot 'experiments/ngx-dlssg-probe/vendor/nvsdk_ngx_s.lib'

$taskArgs = @(
    '/nologo', '/std:c++20', '/EHsc', '/O2', '/MT', '/DNDEBUG', '/DNOMINMAX', '/DWIN32_LEAN_AND_MEAN', '/utf-8', '/W4',
    "/I$taskInclude", "/Fo$taskObject", "/Fe$taskExe", (Join-Path $PSScriptRoot 'main.cpp'),
    '/link', '/INCREMENTAL:NO', '/OPT:REF', '/OPT:ICF', $taskNgx,
    'advapi32.lib', 'user32.lib', 'shell32.lib', 'ole32.lib', 'dxgi.lib', 'd3d12.lib', 'd3dcompiler.lib', 'version.lib', 'bcrypt.lib', 'wintrust.lib', 'psapi.lib', 'shlwapi.lib'
)

& $taskCl @taskArgs 2>&1 | Tee-Object -FilePath "$taskEvidence/build.log" -Append
$taskClExit = $LASTEXITCODE
"CL_EXIT_CODE=$taskClExit" | Add-Content "$taskEvidence/build.log"
if ($taskClExit -ne 0) { throw 'Direct C++ compile/link failed; no second build' }
if (!(Test-Path $taskExe)) { throw 'Executable missing' }
'CPP_BUILD=PASS' | Add-Content "$taskEvidence/build.log"
'LINK=PASS' | Add-Content "$taskEvidence/build.log"

$taskFxc = Join-Path $env:WindowsSdkVerBinPath 'x64/fxc.exe'
foreach ($entry in @('FixtureCS', 'HybridInterpolationCS', 'ReductionCS', 'QualityMetricsCS')) {
    & $taskFxc /nologo /WX /T cs_5_0 /E $entry /Fo "$PSScriptRoot/build/$entry.cso" "$PSScriptRoot/HybridInterpolationCS.hlsl" 2>&1 | Tee-Object -FilePath "$taskEvidence/shader-build.log" -Append
    if ($LASTEXITCODE -ne 0) { throw "Shader build failed: $entry" }
    "${entry}_BUILD=PASS" | Add-Content "$taskEvidence/shader-build.log"
    "${entry}_BUILD=PASS" | Add-Content "$taskEvidence/build.log"
}
'BUILD_ALL=PASS' | Add-Content "$taskEvidence/build.log"
Write-Host "BUILD COMPLETED SUCCESSFULLY"
