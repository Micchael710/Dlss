$ErrorActionPreference='Stop'
$combinedRoot=Split-Path $PSScriptRoot -Parent
$combinedExe=Join-Path $combinedRoot 'experiments/dlssg-vk-d3d12-x2/build/dlssg_vk_d3d12_x2.exe'
$combinedCpu=Get-Content -LiteralPath "$combinedRoot/experiments/dlssg-vk-d3d12-x2/build/cpu-tests/worker-result.json" -Raw | ConvertFrom-Json
if($combinedCpu.self_test -ne 'PASS' -or $combinedCpu.gpu_used -or $combinedCpu.community_loaded) { throw 'CPU tests must pass before the sole GPU run' }
$combinedHistoricalDx=Get-Content -LiteralPath "$combinedRoot/logs/runtime/dlssg-external-harness/20261003-222208-087/reviewed-result.json" -Raw | ConvertFrom-Json
$combinedHistoricalInterop=Get-Content -LiteralPath "$combinedRoot/logs/runtime/dlssg-crossapi-interop/20261003-230013-062/result.json" -Raw | ConvertFrom-Json
if($combinedHistoricalDx.dlssg_sm86_d3d12_x2 -ne 'PASS' -or $combinedHistoricalInterop.cross_api_interop -ne 'PASS') { throw 'Historical closed gates missing; do not rerun them' }
$combinedSourceDll=Join-Path $combinedRoot 'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
$combinedRuntimeDll=Join-Path $combinedRoot 'experiments/ngx-dlssg-probe/vendor/nvngx_dlssg.dll'
if((Get-FileHash $combinedSourceDll).Hash.ToLowerInvariant() -ne 'c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838') { throw 'Component hash mismatch' }
if((Get-FileHash $combinedRuntimeDll).Hash.ToLowerInvariant() -ne 'ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82') { throw 'Runtime hash mismatch' }
$combinedBaselinePaths=@(
 'builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.f0e41bf8.opengl-fg-backpressure.jar',
 'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar',
 'wisteria/native/build/fsr-windows-x64/wisteria_fsr_bridge.dll')
function Get-CombinedBaselineHashes {
 $combinedHashes=[ordered]@{}
 foreach($combinedPath in $combinedBaselinePaths) { $combinedHashes[$combinedPath]=(Get-FileHash -LiteralPath (Join-Path $combinedRoot $combinedPath)).Hash.ToLowerInvariant() }
 return $combinedHashes
}
function Get-CombinedFileEvidence([string]$combinedFile) {
 $combinedInfo=(Get-Item -LiteralPath $combinedFile).VersionInfo
 $combinedSignature=Get-AuthenticodeSignature -LiteralPath $combinedFile
 return [ordered]@{path=$combinedFile;sha256=(Get-FileHash $combinedFile).Hash.ToLowerInvariant();file_version=$combinedInfo.FileVersion;signature_status=[string]$combinedSignature.Status;signer=if($combinedSignature.SignerCertificate){$combinedSignature.SignerCertificate.Subject}else{$null}}
}
$combinedRunId=[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$combinedRun=Join-Path $combinedRoot "logs/runtime/dlssg-vk-d3d12-x2/$combinedRunId"
New-Item -ItemType Directory -Path "$combinedRun/component","$combinedRun/stock-runtime","$combinedRun/backend","$combinedRun/bundle-cache" | Out-Null
$combinedDll=Join-Path $combinedRun 'component/dlssg_sm86.dll'
Copy-Item -LiteralPath $combinedSourceDll -Destination $combinedDll
Copy-Item -LiteralPath $combinedRuntimeDll -Destination "$combinedRun/stock-runtime/nvngx_dlssg.dll"
$combinedManifest=[ordered]@{
 run_id=$combinedRunId;base_commit='bd25bef3ea065b291a743837c140abfd77997bff';utc=[DateTime]::UtcNow.ToString('o');api='VULKAN_D3D12_VULKAN';width=1280;height=720;count=1;index=1;warmup_frames=4;
 strategy='D3D12-owned dedicated shared allocations; Vulkan GPU writes inputs; D3D12 NGX Evaluate; Vulkan G1 readback';
 closed_gates_reused=@('20261003-222208-087','20261003-230013-062');closed_gates_rerun=$false;direct_vulkan_ngx_called=$false;
 community_component=Get-CombinedFileEvidence $combinedDll;component_release='sdli 0.3.5';component_commit='9621db573e07ed54f50c15bbb585ed9a7bdfac28';
 stock_runtime=Get-CombinedFileEvidence "$combinedRun/stock-runtime/nvngx_dlssg.dll";executable=Get-CombinedFileEvidence $combinedExe;
 baseline_before=Get-CombinedBaselineHashes;source_sha256=[ordered]@{};automatic_retries=0;principal_gpu_attempts=1;private_abi_calls=0;capability_overrides=0;optiscaler_loaded=$false;driver_profile_changes_requested=$false;minecraft_launched=$false;mfg_higher_than_x2=$false;cpu_tests='PASS';
}
foreach($combinedSource in @('experiments/dlssg-vk-d3d12-x2/main.cpp','experiments/dlssg-vk-d3d12-x2/CMakeLists.txt','experiments/dlssg-external-harness/harness.cpp','experiments/dlssg-external-harness/d3d12_worker.inc','experiments/dlssg-external-harness/session_setup.inc','tools/build_dlssg_vk_d3d12_x2.ps1','tools/run_dlssg_vk_d3d12_x2.ps1','tools/summarize_dlssg_vk_d3d12_x2.py','tools/dlssg_log_evidence.py')) { $combinedManifest.source_sha256[$combinedSource]=(Get-FileHash (Join-Path $combinedRoot $combinedSource)).Hash.ToLowerInvariant() }
$combinedManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$combinedRun/run-manifest.json" -Encoding UTF8
Write-Output "RUN_DIRECTORY=$combinedRun"
# One hidden worker process. No retries, variants, fallback or Minecraft.
$combinedArgs='"'+$combinedRun+'" "'+$combinedDll+'" "'+(Join-Path $combinedRun 'stock-runtime')+'"'
$combinedProcess=Start-Process -FilePath $combinedExe -ArgumentList $combinedArgs -WindowStyle Hidden -WorkingDirectory $combinedRun -PassThru -RedirectStandardOutput "$combinedRun/stdout.log" -RedirectStandardError "$combinedRun/stderr.log"
$null=$combinedProcess.Handle
$combinedTimeout=-not $combinedProcess.WaitForExit(180000)
if($combinedTimeout) { $combinedProcess.Kill(); $combinedProcess.WaitForExit() }
$combinedManifest.worker_exit_code=$combinedProcess.ExitCode
$combinedManifest.worker_timeout=$combinedTimeout
$combinedManifest.completed_utc=[DateTime]::UtcNow.ToString('o')
$combinedManifest.baseline_after=Get-CombinedBaselineHashes
$combinedManifest.effective_module_files=@()
if(Test-Path -LiteralPath "$combinedRun/worker-result.json") {
 $combinedWorker=Get-Content -LiteralPath "$combinedRun/worker-result.json" -Raw | ConvertFrom-Json
 foreach($combinedModule in $combinedWorker.modules_final) {
  if($combinedModule -match '(nvngx_dlssg|sm86_backend|_nvngx)\.' -and (Test-Path -LiteralPath $combinedModule)) { $combinedManifest.effective_module_files += Get-CombinedFileEvidence $combinedModule }
 }
}
$combinedManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$combinedRun/run-manifest.json" -Encoding UTF8
& 'D:/Python311/python.exe' "$PSScriptRoot/summarize_dlssg_vk_d3d12_x2.py" $combinedRun
if($LASTEXITCODE -ne 0) { throw "Evidence reduction failed; preserve $combinedRun; no GPU retry" }
Write-Output "WORKER_EXIT=$($combinedProcess.ExitCode)"
if($combinedTimeout) { throw "Principal worker timed out; no retry. Evidence: $combinedRun" }
