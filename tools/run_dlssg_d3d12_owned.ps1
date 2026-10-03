$ErrorActionPreference='Stop'
$ownedRoot=Split-Path $PSScriptRoot -Parent
$ownedExe=Join-Path $ownedRoot 'experiments/dlssg-crossapi-interop/build/dlssg_d3d12_owned.exe'
# CPU entry point creates no device or resource and loads no community component.
& $ownedExe --self-test
if ($LASTEXITCODE -ne 0) { throw 'CPU memory intersection/pattern tests failed; no GPU run' }
$ownedRunId=[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$ownedRun=Join-Path $ownedRoot "logs/runtime/dlssg-crossapi-interop/$ownedRunId"
New-Item -ItemType Directory -Path $ownedRun | Out-Null
$ownedManifest=[ordered]@{run_id=$ownedRunId; base_commit='df13071b6cfd02fcbd501fcae1d0d0a09ef99e1f'; utc=[DateTime]::UtcNow.ToString('o'); strategy='D3D12_OWNED_RESOURCE_VULKAN_IMPORT'; width=256; height=256; format='RGBA8_UNORM'; d3d12_flags='ALLOW_RENDER_TARGET'; vulkan_usage='TRANSFER_SRC|TRANSFER_DST|COLOR_ATTACHMENT'; memory_handle='D3D12_RESOURCE'; sync='D3D12_FENCE imported TIMELINE'; cpu_self_tests='PASS'; automatic_retries=0; dlssg_executed=$false; minecraft_launched=$false; vulkan_native_export_attempted=$false; d3d12_heap_attempted=$false; transport_cpu_copy_count=0; exe_sha256=(Get-FileHash $ownedExe).Hash.ToLowerInvariant(); source_sha256=[ordered]@{} }
foreach($ownedSource in @('experiments/dlssg-crossapi-interop/owned.cpp','experiments/dlssg-crossapi-interop/probe.cpp','experiments/dlssg-crossapi-interop/CMakeLists.txt')) { $ownedManifest.source_sha256[$ownedSource]=(Get-FileHash (Join-Path $ownedRoot $ownedSource)).Hash.ToLowerInvariant() }
$ownedManifest | ConvertTo-Json -Depth 6 | Set-Content "$ownedRun/run-manifest.json" -Encoding UTF8
Write-Output "RUN_DIRECTORY=$ownedRun"
$ownedProcess=Start-Process -FilePath $ownedExe -ArgumentList $ownedRun -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ownedRun/stdout.log" -RedirectStandardError "$ownedRun/stderr.log"
$null=$ownedProcess.Handle
$ownedTimeout=-not $ownedProcess.WaitForExit(60000)
if($ownedTimeout) { $ownedProcess.Kill(); $ownedProcess.WaitForExit() }
$ownedManifest.worker_exit_code=$ownedProcess.ExitCode
$ownedManifest.worker_timeout=$ownedTimeout
$ownedManifest.completed_utc=[DateTime]::UtcNow.ToString('o')
$ownedManifest | ConvertTo-Json -Depth 6 | Set-Content "$ownedRun/run-manifest.json" -Encoding UTF8
if (Test-Path "$ownedRun/result.json") { Get-Content "$ownedRun/result.json" }
Write-Output "WORKER_EXIT=$($ownedProcess.ExitCode)"
if($ownedTimeout) { throw "Worker timeout; no retry. Evidence: $ownedRun" }
