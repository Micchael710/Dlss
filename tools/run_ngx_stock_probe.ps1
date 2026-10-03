$ErrorActionPreference = 'Stop'
$probeRoot = Split-Path $PSScriptRoot -Parent
$probeVendor = Join-Path $probeRoot 'experiments/ngx-dlssg-probe/vendor'
$probeBuild = Join-Path $probeRoot 'experiments/ngx-dlssg-probe/build'
$probeRuntime = Join-Path $probeBuild 'nvngx_dlssg.dll'
$probeManifest = Get-Content -LiteralPath "$probeVendor/manifest.json" -Raw | ConvertFrom-Json
$probeExpected = ($probeManifest | Where-Object { $_.path -eq 'lib/Windows_x86_64/rel/nvngx_dlssg.dll' }).sha256
$probeHash = (Get-FileHash -LiteralPath $probeRuntime -Algorithm SHA256).Hash.ToLowerInvariant()
if ($probeHash -ne $probeExpected) { throw 'Stock runtime hash mismatch' }
$probeSignature = Get-AuthenticodeSignature -LiteralPath $probeRuntime
if ($probeSignature.Status -ne 'Valid' -or $probeSignature.SignerCertificate.Subject -notmatch 'NVIDIA Corporation') { throw 'Stock NVIDIA signature verification failed' }
$probeRunId = Get-Date -Format 'yyyyMMdd-HHmmss'
$probeOutput = Join-Path $probeRoot "logs/runtime/ngx-stock-probe/$probeRunId"
New-Item -ItemType Directory -Path $probeOutput -Force | Out-Null
$probeIdentity = [ordered]@{
    scenario = 'stock'
    runtime_path = $probeRuntime
    runtime_version = (Get-Item -LiteralPath $probeRuntime).VersionInfo.FileVersion
    runtime_sha256 = $probeHash
    runtime_signature = [string]$probeSignature.Status
    runtime_signer = $probeSignature.SignerCertificate.Subject
    source_commit = ($probeManifest | Select-Object -First 1).commit
    probe_sha256 = (Get-FileHash -LiteralPath "$probeBuild/ngx_stock_probe.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
    captured_at_utc = (Get-Date).ToUniversalTime().ToString('o')
    sm86_adapted_probe = 'NOT_RUN: reproducible Vulkan backend unavailable'
}
$probeIdentity | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$probeOutput/runtime-identity.json" -Encoding UTF8
& "$probeBuild/ngx_stock_probe.exe" $probeOutput $probeBuild | Tee-Object -FilePath "$probeOutput/console.log"
$probeExit = $LASTEXITCODE
Write-Output "Probe output: $probeOutput"
Write-Output "Probe exit: $probeExit"
if ($probeExit -ne 0) { throw "Probe blocked or getter failed; inspect $probeOutput/raw-result.json" }
