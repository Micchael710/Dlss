$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$taskEvidence = Join-Path $taskRoot 'logs/research/hybrid-mfg-x4-depth-contract'
$taskCounter = Join-Path $PSScriptRoot 'build/run-count.txt'
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null

$taskCount = if (Test-Path $taskCounter) { [int](Get-Content $taskCounter) } else { 0 }
if ($taskCount -ge 1) { throw 'RUN_COUNT_THIS_ITERATION limit reached (1); STOP' }
($taskCount + 1) | Set-Content $taskCounter

$taskExe = Join-Path $PSScriptRoot 'build/hybrid_mfg_x4_depth_contract.exe'
$taskDll = Join-Path $taskRoot 'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
$taskRuntime = Join-Path $taskRoot 'experiments/ngx-dlssg-probe/vendor'
$taskShaders = Join-Path $PSScriptRoot 'build'

if (!(Test-Path $taskExe)) { throw 'Executable missing; build first' }
if (!(Test-Path $taskDll)) { throw 'Loader DLL missing' }
if (!(Test-Path (Join-Path $taskRuntime 'nvngx_dlssg.dll'))) { throw 'NGX runtime missing' }

foreach ($entry in @('FixtureCS', 'HybridInterpolationCS', 'ReductionCS', 'QualityMetricsCS')) {
    if (!(Test-Path (Join-Path $taskShaders "$entry.cso"))) { throw "Shader bytecode missing: $entry" }
}

"RUN_COUNT_THIS_ITERATION=$($taskCount + 1)" | Add-Content "$taskEvidence/run.log"
"EXECUTABLE=$taskExe" | Add-Content "$taskEvidence/run.log"
"LOADER_DLL=$taskDll" | Add-Content "$taskEvidence/run.log"
"RUNTIME_DIR=$taskRuntime" | Add-Content "$taskEvidence/run.log"
"SHADERS_DIR=$taskShaders" | Add-Content "$taskEvidence/run.log"

$startTime = Get-Date
& $taskExe $taskEvidence $taskDll $taskRuntime $taskShaders 2>&1 | Tee-Object -FilePath "$taskEvidence/execution.log"
$runExit = $LASTEXITCODE
$endTime = Get-Date

"EXIT_CODE=$runExit" | Add-Content "$taskEvidence/run.log"
"DURATION_MS=$((New-TimeSpan -Start $startTime -End $endTime).TotalMilliseconds)" | Add-Content "$taskEvidence/run.log"

if ($runExit -ne 0) {
    Write-Host "RUN RETURNED NON-ZERO EXIT CODE: $runExit"
} else {
    Write-Host "RUN FINISHED SUCCESSFULLY WITH EXIT CODE: 0"
}
