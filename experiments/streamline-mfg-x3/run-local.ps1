param([Parameter(Mandatory)][string]$LogDirectory,[ValidateSet('1','2','base','off','reflex','requirements')][string]$GeneratedCount=1)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$taskRuntime=Join-Path $taskRoot 'experiments/streamline-mfg-capability/runtime'
$taskExe=Join-Path $taskRoot 'builds/experimental/streamline-mfg-x3/streamline_mfg_x3.exe'
$taskRun=[IO.Path]::GetFullPath($LogDirectory)
New-Item -ItemType Directory -Force $taskRun | Out-Null
if(Test-Path (Join-Path $taskRun 'launch-requested.json')){throw 'This run already has an attempt; no retry'}
$taskStaged=Get-Content (Join-Path $taskRoot 'logs/research/streamline-mfg-capability/20261004-sdk212/staged-sdk.json') -Raw | ConvertFrom-Json
foreach($taskFile in $taskStaged.files){if((Get-FileHash (Join-Path $taskRuntime $taskFile.name) -Algorithm SHA256).Hash.ToLower() -ne $taskFile.sha256){throw ('Runtime hash mismatch '+$taskFile.name)}}
# Ordinary local user launch: no child-process policy, mitigation or debug layer.
$taskIni=Join-Path $taskRuntime 'component/dlssg_sm86.ini'
$taskOriginal=[IO.File]::ReadAllBytes($taskIni)
$taskBefore=(Get-FileHash $taskIni -Algorithm SHA256).Hash.ToLower()
$taskText=[Text.Encoding]::UTF8.GetString($taskOriginal)
if(([regex]::Matches($taskText,'SpoofArchToGame=0')).Count -ne 1){throw 'Unexpected spoof config'}
@{attempts=1;requested_generated_count=$GeneratedCount;execution_mode='NORMAL_LOCAL';child_process_policy='NONE';runtime_binary_hashes_unchanged=$true;executable_sha256=(Get-FileHash $taskExe -Algorithm SHA256).Hash.ToLower()} | ConvertTo-Json | Set-Content (Join-Path $taskRun 'launch-requested.json')
try {
 if($GeneratedCount -notin @('base','reflex')){[IO.File]::WriteAllBytes($taskIni,[Text.Encoding]::UTF8.GetBytes($taskText.Replace('SpoofArchToGame=0','SpoofArchToGame=1')))}
 $taskProcess=Start-Process -FilePath $taskExe -ArgumentList @($taskRuntime,$taskRun,$GeneratedCount) -WorkingDirectory $taskRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $taskRun 'harness.log') -RedirectStandardError (Join-Path $taskRun 'stderr.log') -PassThru; $taskProcess.WaitForExit()
 @{process_id=$taskProcess.Id;process_exit_code=$taskProcess.ExitCode;attempts=1;execution_mode='NORMAL_LOCAL'} | ConvertTo-Json | Set-Content (Join-Path $taskRun 'process-result.json')
} finally {
 [IO.File]::WriteAllBytes($taskIni,$taskOriginal)
 $taskAfter=(Get-FileHash $taskIni -Algorithm SHA256).Hash.ToLower()
 @{before_sha256=$taskBefore;restored_sha256=$taskAfter;restored_byte_identical=($taskBefore -eq $taskAfter)} | ConvertTo-Json | Set-Content (Join-Path $taskRun 'config-restoration.json')
}
Get-Content (Join-Path $taskRun 'harness.log') -Tail 14
Get-Content (Join-Path $taskRun 'process-result.json')
