"""Final requested key report, after commit/push; never changes runtime evidence."""
from pathlib import Path
import json, subprocess, sys
root=Path(__file__).resolve().parents[1]
r=Path((root/'builds/experimental/dlss-sr/current-combined-run.txt').read_text()).resolve()
s=json.loads((r/'runtime-result.json').read_text());f=json.loads((r/'format-preflight.json').read_text())
native=json.loads((r/'native-diagnostic-summary.json').read_text())
git=Path.home()/'.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe'
end=subprocess.check_output([str(git),'rev-parse','HEAD'],cwd=root,text=True).strip()
files=subprocess.check_output([str(git),'diff-tree','--no-commit-id','--name-only','-r','HEAD'],cwd=root,text=True).splitlines()
records=[json.loads(x) for x in (r/'fg-runtime-samples.jsonl').read_text().splitlines()]
outputs={int(e['record']['fields']['realFrameId']):e['record']['fields'] for e in records if e['record']['type']=='OUTPUT_READY'}
def interval(i):
    rows=s['diagnostic_intervals'][str(i)];submitted=next((e for e in rows if e['event']=='QUEUE_SUBMIT_RESULT'),None)
    if not submitted:return 'NOT_OBSERVED'
    return submitted['detail']+'; D3D12_DONE='+str(any(e['event']=='D3D12_DONE' for e in rows))+'; SLOT_RETIRED='+str(any(e['event']=='SLOT_RETIRE' for e in rows))+'; G1_STATUS='+outputs.get(i,{}).get('status','UNKNOWN')
v=dict(GITHUB_START_HEAD='d9648f281d9b9316d9efa615da231b0cd1a864da',GITHUB_END_HEAD=end,DLSS_SR_STANDALONE_RERUN='NO',
    OLD_DEVICE_LOST_STAGE='SECOND_FG_INPUT_VK_QUEUE_SUBMIT_VK_ERROR_DEVICE_LOST',NEW_DEVICE_LOST_STAGE='NOT_REPRODUCED;NEW_BLOCKER=GL_RELEASE_WAIT_AND_NATIVE_SHUTDOWN_CRASH',
    FIRST_INTERVAL_RESULT=interval(1),SECOND_INTERVAL_RESULT=interval(2),THIRD_INTERVAL_RESULT=interval(3),
    MOTION_SOURCE_FORMAT='RG16F (Vk83)',MOTION_DEST_FORMAT='RG32F (Vk103)',MOTION_BLIT_SUPPORTED=f['RG16F_BLIT_SRC'] and f['RG32F_BLIT_DST'],MOTION_CONVERSION_METHOD='GPU_FLOAT_COLOR_NEAREST_BLIT',DEPTH_COPY_SUPPORTED=f['R32F_BLIT_SRC'] and f['R32F_BLIT_DST'],
    BINARY_SEMAPHORE_REUSE_VALIDATED='READINESS_PER_SLOT_CPU_GUARDS_PASS;GPU_SUBMITS_AND_RETIREMENTS_OBSERVED;GL_RELEASE_WAIT_FAIL',
    FENCE_REUSE_VALIDATED='CPU_RESET_GUARD_PASS;NO_VULKAN_RESET_FAILURE_OBSERVED',COMMAND_BUFFER_REUSE_VALIDATED='CPU_GUARDS_PASS;NATIVE_SLOT_RETIREMENT_OBSERVED',POOL_SLOT_REUSE_VALIDATED='D3D_DONE_AND_CALLER_OUTPUT_PRESENT_RETIREMENT_GUARDS_PASS;OBSERVED_RETIREMENT',
    POST_NGX_DEPTH_LAYOUT_CONTRACT='SDK3.4_READ_STATE_RESTORED;CALLER_SHADER_READ_ONLY_OPTIMAL;NOT_MEASURED_LAYOUT',POST_NGX_MOTION_LAYOUT_CONTRACT='SDK3.4_READ_STATE_RESTORED;CALLER_SHADER_READ_ONLY_OPTIMAL;READWRITE_FLAG_IS_USAGE_NOT_LAYOUT',
    SOURCE_LAYOUT_BEFORE='SHADER_READ_ONLY_OPTIMAL(5)_DECLARED',SOURCE_LAYOUT_FOR_COPY='TRANSFER_SRC_OPTIMAL(6)_RECORDED',SOURCE_LAYOUT_AFTER='SHADER_READ_ONLY_OPTIMAL(5)_RECORDED_RESTORE',
    D3D12_READY_VALUES=[next((e['ready'] for e in s['diagnostic_intervals'][str(i)]),None) for i in (1,2,3)],D3D12_DONE_VALUES=[next((e['done'] for e in s['diagnostic_intervals'][str(i)]),None) for i in (1,2,3)],
    VK_EXT_DEVICE_FAULT_AVAILABLE=f['VK_EXT_device_fault'],DEVICE_FAULT_DESCRIPTION='NOT_QUERIED_NO_DEVICE_LOST;FEATURE_ENABLED='+str(native['device_fault_enabled']),
    VK_NV_DIAGNOSTIC_CHECKPOINTS_AVAILABLE=f['VK_NV_device_diagnostic_checkpoints'],LAST_DIAGNOSTIC_CHECKPOINT='NOT_QUERIED_NO_DEVICE_LOST;MARKERS_INSERTED_FIRST_THREE',
    VALIDATION_ENABLED='NO_LAYER_ABSENT',VALIDATION_ERROR_COUNT='NOT_MEASURED',VALIDATION_ERRORS='UNAVAILABLE;GL_SYNC_ERRORS='+str(s['gl_sync_error_count']),
    FG_CREATE_COUNT=s['fg_create_count'],FG_EVALUATE_COUNT=s['fg_evaluate_count'],FG_GPU_COMPLETION_COUNT=s['fg_gpu_completion_count'],FG_GENERATED_PRESENT_COUNT=s['fg_generated_present_count'],
    DEVICE_LOST=s['device_lost'],VK_ERROR='NONE_OBSERVED_QUEUE_SUBMIT_RESULT=0_FIRST_THREE',CPU_TRANSPORT_COPY_COUNT=0,PER_FRAME_CPU_WAIT_BETWEEN_APIS='NO_NEW_WAIT;EXISTING_RING_BACKPRESSURE_RETAINED',PRESENTWORKER_STILL_SOLE_PRESENT_OWNER=True,
    NORMAL_SHUTDOWN=s['normal_shutdown'],CRASH=s['crash'],PROCESS_EXIT_CODE=s['minecraft_process_exit_code'],DLSS_SR_PLUS_DLSSG_X2='FAIL',
    IF_FAIL_STAGE='GL_RELEASE_WAIT;THEN_SHUTDOWN',IF_FAIL_OPERATION='glWaitSemaphoreEXT;PROCESS_FAST_FAIL',IF_FAIL_COMMAND='FrameResourcesSet.awaitCaptureRelease',IF_FAIL_RESOURCE='BORROWED_DEPTH_OR_MOTION_RELEASE;EXACT_HANDLE_NOT_LOGGED_AT_GL_ERROR',IF_FAIL_QUEUE='GL_RENDER_THREAD;NATIVE_FG_QUEUE_SUBMITS_SUCCEEDED',IF_FAIL_FRAME_ID='UNKNOWN_GL_ERROR_CALLBACK_HAS_NO_CAPTURE_ID',
    ROOT_CAUSE='UNKNOWN_FOR_ORIGINAL_DEVICE_LOST_AND_NATIVE_SHUTDOWN;GL_WAIT_FAILURE_OBSERVED',CAUSE_CONFIDENCE='HIGH_OBSERVED_STAGE;UNDERLYING_CAUSE_UNKNOWN',
    AMD_BASELINE_MODIFIED='NO_9_CLASSES_2_DLLS_BYTE_IDENTICAL',MFG_X3_EVIDENCE_PRESERVED='YES',X3_TESTED='NO',X4_TESTED='NO',X5_TESTED='NO',X6_TESTED='NO',DATA_HARDCODED_OR_FABRICATED='NO_GPU_OR_COMPLETION_DATA_FABRICATED;BASELINE_UUID_AND_VERSION_PINNED',
    CONFIG_RESTORATION=s['config_restoration'],FILES_MODIFIED=files,NEW_LOG_DIRECTORIES=[str(r)],GITHUB_BRANCH='main',GITHUB_COMMIT_SHA=end,GITHUB_PUSH_RESULT=sys.argv[1] if len(sys.argv)>1 else 'NOT_VERIFIED',
    SR_EVALUATE_COUNT=s['sr_evaluate_count'],FG_VALID_INTERVAL_COUNT=s['fg_valid_interval_count'],REAL_G1_REAL_OBSERVED=s['real_g1_real_observed'],NO_RETRY=True)
path=root/'builds/experimental/dlss-sr/DLSS_SR_DLSSG_X2_SECOND_SUBMIT_FINAL_REPORT.txt'
path.write_text(''.join(k+'='+ (json.dumps(value,ensure_ascii=False) if isinstance(value,(dict,list,bool)) else str(value))+'\n' for k,value in v.items()),encoding='utf-8')
print(path)
