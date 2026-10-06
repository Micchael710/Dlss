$ErrorActionPreference='Stop'
$taskRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$taskRun=Join-Path $taskRoot 'logs/research/hybrid-mfg-x4-d3d12-fix'
$taskProcessFile=Join-Path $taskRun 'process-result.json'
if(Test-Path $taskProcessFile){
 $taskExisting=Get-Content $taskProcessFile -Raw | ConvertFrom-Json
 if($taskExisting.run_count_this_iteration -ge 1 -or $taskExisting.launched -eq $true){
  throw 'One run already executed or launched; STOP, no retry'
 }
}
$taskExe=Join-Path $PSScriptRoot 'build/hybrid_mfg_x4.exe'
if(!(Test-Path $taskExe)){throw 'Build missing'}
$taskDll=Join-Path $taskRoot 'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
$taskRuntime=Join-Path $taskRoot 'experiments/ngx-dlssg-probe/vendor/nvngx_dlssg.dll'
if((Get-FileHash $taskDll).Hash.ToLowerInvariant() -ne 'c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838'){throw 'Known component mismatch'}
if((Get-FileHash $taskRuntime).Hash.ToLowerInvariant() -ne 'ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82'){throw 'Known NGX runtime mismatch'}
foreach($subDir in @('component','stock-runtime','backend','bundle-cache')){New-Item -ItemType Directory -Path "$taskRun/$subDir" -Force | Out-Null}
Copy-Item -LiteralPath $taskDll -Destination "$taskRun/component/version.dll"
Copy-Item -LiteralPath $taskRuntime -Destination "$taskRun/stock-runtime/nvngx_dlssg.dll"
$taskState=[ordered]@{experiment='HYBRID_MFG_X4_D3D12_VALIDATION_FIX';run_count_this_iteration=1;build_count_this_iteration=1;launched=$true;started_utc=[DateTime]::UtcNow.ToString('o');process_exit_code=$null;timeout=$false;crash=$null;retry_allowed=$false}
$taskState | ConvertTo-Json | Set-Content $taskProcessFile
$taskArgs='"'+$taskRun+'" "'+$taskRun+'/component/version.dll" "'+$taskRun+'/stock-runtime" "'+$PSScriptRoot+'/build"'
$taskWorker=Start-Process -FilePath $taskExe -ArgumentList $taskArgs -WindowStyle Hidden -WorkingDirectory $taskRun -PassThru -RedirectStandardOutput "$taskRun/harness.log" -RedirectStandardError "$taskRun/stderr.log"
$null=$taskWorker.Handle
$taskTimeout=-not $taskWorker.WaitForExit(180000)
if($taskTimeout){$taskWorker.Kill();$taskWorker.WaitForExit()}
$taskState.process_exit_code=$taskWorker.ExitCode
$taskState.timeout=$taskTimeout
$taskState.crash=($taskWorker.ExitCode -lt 0)
$taskState.completed_utc=[DateTime]::UtcNow.ToString('o')
$taskState | ConvertTo-Json | Set-Content $taskProcessFile
if(Test-Path "$taskRun/worker-result.json"){$taskResult=Get-Content "$taskRun/worker-result.json" -Raw | ConvertFrom-Json}else{
 $taskResult=[pscustomobject]@{experiment='HYBRID_MFG_X4_D3D12_VALIDATION_FIX';github_start_head='6e06093c7eaa088bd7122e9649260563c2d6ddeb';hybrid_x4_frameset_generation='FAIL';fail_stage='HARNESS_PROCESS_DID_NOT_RETURN_EVIDENCE';next_experiment='STOP';present_executed=$false;production_quality_interpolation=$false}
}
foreach($entry in @{process_exit_code=$taskWorker.ExitCode;crash=$taskState.crash;timeout=$taskTimeout;run_count_this_iteration=1;build_count_this_iteration=1}.GetEnumerator()){
 $taskResult | Add-Member -MemberType NoteProperty -Name $entry.Key -Value $entry.Value -Force
}
if($taskWorker.ExitCode -ne 0 -or $taskTimeout){$taskResult.hybrid_x4_frameset_generation='FAIL';$taskResult.next_experiment='STOP'}
$taskResult | ConvertTo-Json -Depth 20 | Set-Content "$taskRun/result.json"
if(!(Test-Path "$taskRun/frame-fingerprints.txt")){'NOT_MEASURED: see failure stage' | Set-Content "$taskRun/frame-fingerprints.txt"}
if(!(Test-Path "$taskRun/validation-messages.log")){New-Item -ItemType File -Path "$taskRun/validation-messages.log" -Force | Out-Null}
if(!(Test-Path "$taskRun/d3d12-debug.log")){New-Item -ItemType File -Path "$taskRun/d3d12-debug.log" -Force | Out-Null}
Write-Output "RESULT=$($taskResult.hybrid_x4_frameset_generation) EXIT=$($taskWorker.ExitCode)"

