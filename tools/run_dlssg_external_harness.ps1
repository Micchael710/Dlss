$ErrorActionPreference = 'Stop'
$harnessRoot = Split-Path $PSScriptRoot -Parent
$harnessExe = Join-Path $harnessRoot 'experiments/dlssg-external-harness/build/dlssg_external_harness.exe'
$harnessSourceDll = Join-Path $harnessRoot 'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
$harnessStockDll = Join-Path $harnessRoot 'experiments/ngx-dlssg-probe/vendor/nvngx_dlssg.dll'
if ((Get-FileHash -LiteralPath $harnessSourceDll -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838') { throw 'Identified external module hash mismatch' }
if ((Get-FileHash -LiteralPath $harnessStockDll -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82') { throw 'Official runtime hash mismatch' }
$harnessRunId = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$harnessRun = Join-Path $harnessRoot "logs/runtime/dlssg-external-harness/$harnessRunId"
New-Item -ItemType Directory -Path "$harnessRun/component","$harnessRun/stock-runtime","$harnessRun/backend","$harnessRun/bundle-cache" | Out-Null
$harnessDll = Join-Path $harnessRun 'component/dlssg_sm86.dll'
Copy-Item -LiteralPath $harnessSourceDll -Destination $harnessDll
Copy-Item -LiteralPath $harnessStockDll -Destination "$harnessRun/stock-runtime/nvngx_dlssg.dll"
Copy-Item -LiteralPath (Join-Path $harnessRoot 'logs/runtime/ngx-stock-probe/20261003-141624/result.json') -Destination "$harnessRun/stock-reference.json"
function Get-HarnessFileEvidence([string]$harnessFile) {
 $harnessInfo = (Get-Item -LiteralPath $harnessFile).VersionInfo
 $harnessSignature = Get-AuthenticodeSignature -LiteralPath $harnessFile
 return [ordered]@{ path=$harnessFile; sha256=(Get-FileHash -LiteralPath $harnessFile -Algorithm SHA256).Hash.ToLowerInvariant(); file_version=$harnessInfo.FileVersion; signature_status=[string]$harnessSignature.Status; signer=if($harnessSignature.SignerCertificate){$harnessSignature.SignerCertificate.Subject}else{$null} }
}
$harnessManifest = [ordered]@{
 run_id=$harnessRunId; utc=[DateTime]::UtcNow.ToString('o'); api='Vulkan'; count=1; index=1;
 community_component=Get-HarnessFileEvidence $harnessDll;
 component_repository='sdli1995/dlssg_for_sm86'; component_commit='9621db573e07ed54f50c15bbb585ed9a7bdfac28'; component_release='0.3.5';
 stock_runtime=Get-HarnessFileEvidence "$harnessRun/stock-runtime/nvngx_dlssg.dll";
 executable=Get-HarnessFileEvidence $harnessExe;
 automatic_downloads=0; private_abi_calls=0; capability_overrides=0; optiscaler_loaded=$false;
 minecraft_launched=$false; driver_profile_changes_requested=$false;
 stock_reference='logs/runtime/ngx-stock-probe/20261003-141624/result.json';
 supported_scope='One adapted worker, Vulkan x2, no automatic retry or D3D12';
}
$harnessManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$harnessRun/run-manifest.json" -Encoding UTF8
Write-Output "RUN_DIRECTORY=$harnessRun"
# Coordinator owns the only worker, timeout and abnormal-exit evidence. No second GPU run.
& $harnessExe $harnessRun $harnessDll "$harnessRun/stock-runtime" *> "$harnessRun/coordinator-console.log"
$harnessExit = $LASTEXITCODE
$harnessManifest.coordinator_exit_code=$harnessExit
$harnessManifest.completed_utc=[DateTime]::UtcNow.ToString('o')
$harnessEffectiveModules = @()
if (Test-Path -LiteralPath "$harnessRun/worker-result.json") {
 try {
  $harnessWorker = Get-Content -LiteralPath "$harnessRun/worker-result.json" -Raw | ConvertFrom-Json
  foreach ($harnessModule in $harnessWorker.modules_final) {
   if ($harnessModule -match '(nvngx_dlssg|sm86_backend|_nvngx)\.' -and (Test-Path -LiteralPath $harnessModule)) { $harnessEffectiveModules += Get-HarnessFileEvidence $harnessModule }
  }
 } catch { $harnessManifest.worker_result_parse_error=[string]$_ }
}
$harnessManifest.effective_module_files=$harnessEffectiveModules
$harnessManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$harnessRun/run-manifest.json" -Encoding UTF8
& 'D:/Python311/python.exe' (Join-Path $PSScriptRoot 'summarize_dlssg_external_harness.py') $harnessRun
if ($LASTEXITCODE -ne 0) { throw "Evidence reduction failed; original evidence preserved at $harnessRun" }
Write-Output "WORKER_EXIT=$harnessExit"
