$ErrorActionPreference = 'Stop'
$interopRoot = Split-Path $PSScriptRoot -Parent
$interopExe = Join-Path $interopRoot 'experiments/dlssg-crossapi-interop/build/dlssg_crossapi_interop.exe'
$interopId = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$interopRun = Join-Path $interopRoot "logs/runtime/dlssg-crossapi-interop/$interopId"
New-Item -ItemType Directory -Path $interopRun | Out-Null
$interopManifest = [ordered]@{ run_id=$interopId; utc=[DateTime]::UtcNow.ToString('o'); phase='A_CAPABILITY'; width=256; height=256; format='RGBA8_UNORM'; vulkan_tiling='OPTIMAL'; vulkan_usage='TRANSFER_SRC|TRANSFER_DST'; d3d12_flags='NONE'; exe_sha256=(Get-FileHash $interopExe).Hash.ToLowerInvariant(); source_sha256=(Get-FileHash (Join-Path $interopRoot 'experiments/dlssg-crossapi-interop/probe.cpp')).Hash.ToLowerInvariant(); base_commit='4c3efe98882e68e7b93039f1d868ea07655b7424'; ngx_linked=$false; community_component_loaded=$false; dlssg_executed=$false; minecraft_launched=$false; swapchain_created=$false; global_driver_changes=$false; automatic_retries=0 }
$interopManifest | ConvertTo-Json -Depth 5 | Set-Content "$interopRun/run-manifest.json" -Encoding UTF8
Write-Output "RUN_DIRECTORY=$interopRun"
$interopTimer=[Diagnostics.Stopwatch]::StartNew()
$interopProcess=Start-Process -FilePath $interopExe -ArgumentList $interopRun -WindowStyle Hidden -PassThru -RedirectStandardOutput "$interopRun/stdout.log" -RedirectStandardError "$interopRun/stderr.log"
# Cache the native process handle before waiting so ExitCode remains observable.
$null = $interopProcess.Handle
if (-not $interopProcess.WaitForExit(60000)) { $interopProcess.Kill(); throw "Probe timeout; evidence preserved: $interopRun" }
$interopTimer.Stop()
$interopManifest.worker_exit=$interopProcess.ExitCode
if ($null -eq $interopManifest.worker_exit) { $interopManifest.worker_exit_observation='UNKNOWN; do not infer native exit code from result fields' }
$interopManifest | ConvertTo-Json -Depth 5 | Set-Content "$interopRun/run-manifest.json" -Encoding UTF8
[ordered]@{ method='CPU_WALL_CLOCK_PROBE_ONLY'; probe_wall_ms=$interopTimer.Elapsed.TotalMilliseconds; vulkan_work_ms=$null; d3d12_work_ms=$null; vulkan_to_d3d12_handoff_ms=$null; d3d12_to_vulkan_handoff_ms=$null; roundtrip_ms=$null; gpu_timestamp_comparison='NOT_RUN; no GPU dispatch in capability probe' } | ConvertTo-Json | Set-Content "$interopRun/timings.json" -Encoding UTF8
[ordered]@{ phase='A'; texture_size='256x256'; d3d12_shared_committed_resource='See result'; functional_shared_allocation=$false; cross_api_import_or_export_executed=$false; handles_creator='D3D12 CreateSharedHandle'; handles_owner='application'; handles_closed='after D3D12 OpenSharedHandle'; cpu_transport_copies=0; gpu_copies=0 } | ConvertTo-Json | Set-Content "$interopRun/resource-map.json" -Encoding UTF8
[ordered]@{ fence_share_creation='See result'; vulkan_semaphore_import=$false; queue_work_submitted=$false; gpu_ordering_test='NOT_RUN' } | ConvertTo-Json | Set-Content "$interopRun/sync-map.json" -Encoding UTF8
[ordered]@{ status='NOT_RUN'; reason='Capability probe has no pixel writes/readbacks' } | ConvertTo-Json | Set-Content "$interopRun/hashes.json" -Encoding UTF8
Get-Content "$interopRun/result.json"
Write-Output "WORKER_EXIT=$($interopProcess.ExitCode)"
