param([Parameter(Mandatory)][string]$LogDirectory,[ValidateSet(1,2)][int]$GeneratedCount=1)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$taskRuntime=Join-Path $taskRoot 'experiments/streamline-mfg-capability/runtime'
$taskExe=Join-Path $taskRoot 'builds/experimental/streamline-mfg-x3/streamline_mfg_x3.exe'
$taskRun=[IO.Path]::GetFullPath($LogDirectory)
New-Item -ItemType Directory -Force $taskRun | Out-Null
if(Test-Path (Join-Path $taskRun 'launch-requested.json')){throw 'This run already has an attempt; no retry'}
$taskStaged=Get-Content (Join-Path $taskRoot 'logs/research/streamline-mfg-capability/20261004-sdk212/staged-sdk.json') -Raw | ConvertFrom-Json
foreach($taskFile in $taskStaged.files){if((Get-FileHash (Join-Path $taskRuntime $taskFile.name) -Algorithm SHA256).Hash.ToLower() -ne $taskFile.sha256){throw ('Runtime hash mismatch '+$taskFile.name)}}
# Normal local user process, with only child-process creation disabled to prevent
# SL's mandatory updater bootstrap. No filesystem/GPU/HWND sandbox or DLL changes.
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class SLNormalLocal {
 [StructLayout(LayoutKind.Sequential)] struct SA { public int size; public IntPtr descriptor; [MarshalAs(UnmanagedType.Bool)] public bool inherit; }
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct SI { public int cb; public string reserved,desktop,title; public int x,y,xSize,ySize,xChars,yChars,fill,flags; public short show,reserved2; public IntPtr reservedPtr,input,output,error; }
 [StructLayout(LayoutKind.Sequential)] struct SIX { public SI start; public IntPtr attributes; }
 [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr process,thread; public uint pid,tid; }
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool InitializeProcThreadAttributeList(IntPtr list,int count,int flags,ref IntPtr size);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool UpdateProcThreadAttribute(IntPtr list,uint flags,IntPtr attribute,IntPtr value,IntPtr size,IntPtr previous,IntPtr returned);
 [DllImport("kernel32.dll")] static extern void DeleteProcThreadAttributeList(IntPtr list);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessW(string app,StringBuilder command,IntPtr pAttr,IntPtr tAttr,bool inherit,uint flags,IntPtr env,string cwd,ref SIX start,out PI process);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateFileW(string path,uint access,uint share,ref SA sa,uint disposition,uint flags,IntPtr template);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle,uint timeout);
 [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 static void Check(bool success){if(!success)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());}
 public static uint[] Run(string exe,string runtime,string logs,int count) {
  IntPtr size=IntPtr.Zero,attrs=IntPtr.Zero,policy=IntPtr.Zero,stdout=IntPtr.Zero,stderr=IntPtr.Zero,input=IntPtr.Zero; PI pi=new PI(); bool ready=false;
  try {
   InitializeProcThreadAttributeList(IntPtr.Zero,1,0,ref size);attrs=Marshal.AllocHGlobal(size);Check(InitializeProcThreadAttributeList(attrs,1,0,ref size));ready=true;
   policy=Marshal.AllocHGlobal(4);Marshal.WriteInt32(policy,1);Check(UpdateProcThreadAttribute(attrs,0,new IntPtr(0x2000E),policy,new IntPtr(4),IntPtr.Zero,IntPtr.Zero));
   SA sa=new SA();sa.size=Marshal.SizeOf(sa);sa.inherit=true;
   stdout=CreateFileW(System.IO.Path.Combine(logs,"harness.log"),0x40000000,3,ref sa,2,0x80,IntPtr.Zero);
   stderr=CreateFileW(System.IO.Path.Combine(logs,"stderr.log"),0x40000000,3,ref sa,2,0x80,IntPtr.Zero);
   input=CreateFileW("NUL",0x80000000,3,ref sa,3,0x80,IntPtr.Zero);
   Check(stdout!=new IntPtr(-1)&&stderr!=new IntPtr(-1)&&input!=new IntPtr(-1));
   SIX si=new SIX();si.start.cb=Marshal.SizeOf(si);si.start.flags=0x100;si.start.input=input;si.start.output=stdout;si.start.error=stderr;si.attributes=attrs;
   StringBuilder cmd=new StringBuilder("\""+exe+"\" \""+runtime+"\" \""+logs+"\" "+count);
   Check(CreateProcessW(exe,cmd,IntPtr.Zero,IntPtr.Zero,true,0x08080000,IntPtr.Zero,runtime,ref si,out pi));
   Check(WaitForSingleObject(pi.process,0xFFFFFFFF)==0);uint exit;Check(GetExitCodeProcess(pi.process,out exit));return new uint[]{pi.pid,exit};
  } finally {if(pi.thread!=IntPtr.Zero)CloseHandle(pi.thread);if(pi.process!=IntPtr.Zero)CloseHandle(pi.process);foreach(IntPtr h in new[]{stdout,stderr,input})if(h!=IntPtr.Zero&&h!=new IntPtr(-1))CloseHandle(h);if(ready)DeleteProcThreadAttributeList(attrs);if(attrs!=IntPtr.Zero)Marshal.FreeHGlobal(attrs);if(policy!=IntPtr.Zero)Marshal.FreeHGlobal(policy);}
 }
}
'@
$taskIni=Join-Path $taskRuntime 'component/dlssg_sm86.ini'
$taskOriginal=[IO.File]::ReadAllBytes($taskIni)
$taskBefore=(Get-FileHash $taskIni -Algorithm SHA256).Hash.ToLower()
$taskText=[Text.Encoding]::UTF8.GetString($taskOriginal)
if(([regex]::Matches($taskText,'SpoofArchToGame=0')).Count -ne 1){throw 'Unexpected spoof config'}
@{attempts=1;requested_generated_count=$GeneratedCount;execution_mode='NORMAL_LOCAL';child_process_policy='RESTRICTED_FOR_NO_OTA_ONLY';runtime_binary_hashes_unchanged=$true;executable_sha256=(Get-FileHash $taskExe -Algorithm SHA256).Hash.ToLower()} | ConvertTo-Json | Set-Content (Join-Path $taskRun 'launch-requested.json')
try {
 [IO.File]::WriteAllBytes($taskIni,[Text.Encoding]::UTF8.GetBytes($taskText.Replace('SpoofArchToGame=0','SpoofArchToGame=1')))
 $taskResult=[SLNormalLocal]::Run($taskExe,$taskRuntime,$taskRun,$GeneratedCount)
 @{process_id=$taskResult[0];process_exit_code=$taskResult[1];attempts=1;execution_mode='NORMAL_LOCAL'} | ConvertTo-Json | Set-Content (Join-Path $taskRun 'process-result.json')
} finally {
 [IO.File]::WriteAllBytes($taskIni,$taskOriginal)
 $taskAfter=(Get-FileHash $taskIni -Algorithm SHA256).Hash.ToLower()
 @{before_sha256=$taskBefore;restored_sha256=$taskAfter;restored_byte_identical=($taskBefore -eq $taskAfter)} | ConvertTo-Json | Set-Content (Join-Path $taskRun 'config-restoration.json')
}
Get-Content (Join-Path $taskRun 'harness.log') -Tail 14
Get-Content (Join-Path $taskRun 'process-result.json')
