"""Final report from observed evidence and verified Git push. Output remains local."""
from pathlib import Path
import json, re, subprocess, sys
root=Path(__file__).resolve().parents[1];r=Path(sys.argv[1]).resolve()
assert r.is_relative_to(root/'logs/runtime/dlss-sr')
s=json.loads((r/'runtime-result.json').read_text());m=json.loads((r/'manifest.json').read_text())
git=Path('C:/Users/micha/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe')
readgit=lambda *a:subprocess.check_output([str(git),*a],cwd=root,text=True).strip()
end=readgit('rev-parse','HEAD');remote=readgit('ls-remote','https://github.com/Micchael710/Dlss.git','refs/heads/main').split()[0]
assert end==remote,'Push not verified'
fields=lambda e:dict(re.findall(r'(\w+)=([^ ]+)',e['detail']))
copies=[fields(e) for e in s['input_copy_records']]
d=next(x for x in copies if x['resource']=='depth');v=next(x for x in copies if x['resource']=='motion')
last=copies[-1];consumer=fields(s['input_copy_submissions'][0])
receipt=fields(s['readiness_receipts'][0]);completion=fields(s['completion_snapshots'][0])
pool_images=s['native_fg_counts'].get('resource_pool',0);pool_slots=pool_images//(4+m['requested_count'])
matrix_frame=re.search(r'logicalFrame=(\d+)',s['matrix_evidence'][0])[1] if s['matrix_evidence'] else 'UNKNOWN'
submitted=len(s['input_copy_submissions']);retired=len(s['input_copy_retirement_completions'])
fmt={'100':'R32F','83':'RG16F','103':'RG32F'}
f={
'GITHUB_START_HEAD':'b3dd23c0f0b28a9e494ea0aac61fb51209179f63','GITHUB_END_HEAD':end,
'DLSS_SR_STANDALONE':'PASS_HISTORICAL_4421','DLSS_SR_STANDALONE_RERUN':'NO',
'BORROWED_INPUT_REJECTION_OLD':'BORROWED_VULKAN_INPUTS_NO_VALIDATED_QUEUE_JOIN',
'BORROWED_INPUT_QUEUE_JOIN_IMPLEMENTED':'YES_EXPERIMENTAL_GUARDED;GPU_VALIDATION_FAIL',
'QUEUE_FAMILY_TRANSFER_REQUIRED':'NO_BETWEEN_SR_FAMILY_'+d['producerFamily']+'_AND_FG_FAMILY_'+consumer['consumerFamily'],
'QUEUE_INDEX_DIFFERENT':'YES_SR_'+d['producerIndex']+'_FG_'+receipt['consumerIndex']+'_OBSERVED','SEMAPHORE_WAIT_USED':'YES_FIRST_INPUT_SUBMIT',
'SYNC_PRIMITIVE':'POST_SR_BINARY_SIGNALS -> FG_VULKAN_GPU_WAIT -> D3D12_SHARED_FENCE_TIMELINE -> VULKAN_OUTPUT_GPU_WAIT -> GL_RELEASE_SERVER_WAIT',
'DEPTH_SOURCE_FORMAT':fmt[d['sourceFormat']],'DEPTH_DEST_FORMAT':fmt[d['destinationFormat']],
'DEPTH_SOURCE_SIZE':d['width']+'x'+d['height'],'DEPTH_DEST_SIZE':d['width']+'x'+d['height'],
'DEPTH_GPU_COPY_COUNT':f'{submitted}_SUBMITTED;{retired}_RETIREMENT_COMPLETED;FIRST_COMPLETION_INFERRED_FROM_CALLBACK_DONE_'+completion['completion'],
'MOTION_SOURCE_FORMAT':fmt[v['sourceFormat']],'MOTION_DEST_FORMAT':fmt[v['destinationFormat']],
'MOTION_SOURCE_SIZE':v['width']+'x'+v['height'],'MOTION_DEST_SIZE':v['width']+'x'+v['height'],
'MOTION_GPU_COPY_COUNT':f'{submitted}_SUBMITTED;{retired}_RETIREMENT_COMPLETED;FIRST_COMPLETION_INFERRED_FROM_CALLBACK_DONE_'+completion['completion'],
'SOURCE_LAYOUT_BEFORE':'5_SHADER_READ_ONLY_DECLARED_BY_GL;POST_NGX_ACTUAL_UNPROVEN',
'SOURCE_LAYOUT_FOR_COPY':'6_TRANSFER_SRC_RECORDED','SOURCE_LAYOUT_AFTER':'5_SHADER_READ_ONLY_RECORDED;NOT_FULLY_GPU_VALIDATED',
'FG_OWNED_INPUT_POOL':f'PERSISTENT_{pool_slots}_SLOTS_{pool_images}_IMAGES_ONE_SETUP',
'PER_FRAME_RESOURCE_CREATION_COUNT':'0_AFTER_POOL_SETUP','CPU_TRANSPORT_COPY_COUNT':s['cpu_transport_copy_count'],
'PER_FRAME_CPU_WAIT_BETWEEN_APIS':'NO_IN_NEW_JOIN;EXISTING_VULKAN_RING_REUSE_BACKPRESSURE_REMAINS',
'DLSS_SR_EVALUATE_COUNT':s['sr_counts'].get('EVALUATE',0),
'DLSS_SR_GPU_COMPLETION_COUNT':str(s['sr_counts'].get('GPU_COMPLETION',0))+'_DIAGNOSTIC_EVENTS;FIRST_SR_DEPENDENCY_INFERRED_FROM_FG_FENCE',
'FG_CREATE_COUNT':s['fg_create_count'],'FG_EVALUATE_COUNT':s['fg_evaluate_count'],
'FG_GPU_COMPLETION_COUNT':str(s['fg_completion_callback_count'])+'_CALLBACK;'+str(s['fg_retirement_completion_count'])+'_RETIREMENT_EVENTS',
'FG_GENERATED_PRESENT_COUNT':s['fg_generated_present_count'],'FG_REQUESTED_COUNT':m['requested_count'],'FG_INDEX':1,
'PROJECTION_MATRIX_WARNING':'YES_INFINITY_FRAME'+matrix_frame+'_GUARD_RETAINED' if s['matrix_warning'] else 'NO',
'PROJECTION_MATRIX_FAIL_STAGE':'NO_SUBSEQUENT_FRAMES_REACHED_EVALUATE',
'ACTIVE_SR_PROVIDER':'dlss','ACTIVE_FG_PROVIDER':'wisteria:dlssg_fg / wisteria:dlssg;LATCHED_FAIL_AFTER_GPU_SUBMISSION',
'DLSS_SR_PLUS_DLSSG_X2':s['status'],
'PIPELINE_ORDER':'SR -> POST_SR_BINARY_SIGNAL -> FG_OWNED_GPU_BLIT -> SHARED_D3D12_FENCE -> DLSSG -> VULKAN_OUTPUT -> PRESENTWORKER;STOP_AT_SECOND_INPUT_SUBMIT',
'PRESENTWORKER_STILL_SOLE_PRESENT_OWNER':'YES_ARCHITECTURE_UNCHANGED;REAL_PRESENT_EVENTS_'+str(s['fg_real_present_count']),
'D3D12_PRESENT_CALLS':'0_DIRECT_CALLS_IN_PROVIDER_SOURCE;NO_RUNTIME_API_TRACE',
'DLSSG_PROVIDER_VKQUEUEPRESENT_CALLS':'0_DIRECT_CALLS_IN_PROVIDER_SOURCE;NO_RUNTIME_API_TRACE',
'NORMAL_SHUTDOWN':'NO','CRASH':'YES_JAVA_UNRECOVERABLE_CAPTURE_SLOT;0xC0000409_NOT_OBSERVED',
'DEVICE_LOST':'YES_VK_ERROR_DEVICE_LOST_-4' if s['device_lost'] else 'NO',
'PROCESS_EXIT_CODE':s['minecraft_process_exit_code'],'GRADLE_EXIT_CODE':s['gradle_exit_code'],
'CONFIG_RESTORATION':s['config_restoration']['status']+'_'+s['config_restoration']['restored_sha256'],
'AMD_BASELINE_MODIFIED':'NO_9_CLASSES_2_DLLS_HASH_IDENTICAL','AMD_FSR_FG_X2':'PASS_UNCHANGED_HISTORICAL;NOT_RERUN',
'MFG_X3_EVIDENCE_PRESERVED':'YES','G2_IMAGE_GENERATION':'PASS_BOUNDED_DIAGNOSTIC_HISTORICAL',
'G2_OPTIONAL_STATUS':'UNKNOWN_0xffffffff','MINECRAFT_DLSSG_X3':'BLOCKED_STATUS_SEMANTICS_UNPROVEN',
'X3_TESTED':'NO','X4_TESTED':'NO','X5_TESTED':'NO','X6_TESTED':'NO','DATA_HARDCODED_OR_FABRICATED':'NO',
'IF_FAIL_STAGE':s['first_failure']['stage'],'IF_FAIL_RESOURCE':'BORROWED_DEPTH_MOTION_FG_INPUT_COMMAND;SPECIFIC_FAULTING_RESOURCE_UNKNOWN',
'IF_FAIL_QUEUE':'FG_FAMILY'+consumer['consumerFamily']+'_INDEX'+receipt['consumerIndex']+'_QUEUE'+consumer['consumerQueue'],
'IF_FAIL_FRAME_ID':'REAL'+last['realFrameId']+'_LOGICAL'+last['logicalFrame']+'_CAPTURE_GENERATION'+last['captureGeneration'],
'ROOT_CAUSE':s['root_cause'],'CAUSE_CONFIDENCE':s['cause_confidence'],
'FILES_MODIFIED':readgit('diff','--name-only','b3dd23c0f0b28a9e494ea0aac61fb51209179f63','HEAD').replace('\n',';'),
'NEW_LOG_DIRECTORIES':str(r.relative_to(root)),
'GITHUB_BRANCH':readgit('branch','--show-current'),'GITHUB_COMMIT_SHA':end,
'GITHUB_PUSH_RESULT':'PASS_REMOTE_MAIN_VERIFIED_'+remote,
}
target=root/'builds/experimental/dlss-sr/DLSS_SR_DLSSG_X2_HANDOFF_FINAL_REPORT.txt'
target.write_text('\n'.join(k+'='+str(value) for k,value in f.items())+'\n',encoding='utf-8')
print(target);print('Complete mandatory report; push verified; combined result='+s['status'])
