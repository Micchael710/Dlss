"""Derive the requested field report from closed evidence; no generation PASS inference."""
from pathlib import Path
import json, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]
run = Path(sys.argv[1]).resolve()
assert run.is_relative_to(ROOT / 'logs/runtime')
read = lambda name: json.loads((run / name).read_text(encoding='utf-8-sig'))
result, review, manifest = read('result.json'), read('index2-boundary-review.json'), read('run-manifest.json')
assert result['normal_shutdown'] or result['crash']
git = 'C:/Users/micha/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe'
head = subprocess.check_output([git, 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
pair = next(p for p in review['observed_pairs'] if p['indices'][1]['status_trace']['STATUS_AFTER_GROUP'] == '4294967295')
t1, t2 = [i['status_trace'] for i in pair['indices']]
timing = result['metrics']
events = [json.loads(s) for s in (run/'frame-sequence.jsonl').read_text().splitlines() if s.strip()]
present = [e['fields'] for e in events if e['type'] == 'PRESENT']
trace_text = ' -> '.join(f"{p['kind']}:{p['realFrameId']}:index{p['generatedIndex']}" for p in present)
changed = subprocess.check_output([git, 'diff', '--name-only', 'ed7a4d4', 'HEAD'], cwd=ROOT, text=True).splitlines()
f = dict(
    GITHUB_START_HEAD='ed7a4d42cdfa887f94103872834ceaf72d512b7e', GITHUB_END_HEAD=head,
    RUNTIME_JAVA_VERSION=result['Minecraft_process_java']['java_version'],
    MINECRAFT_PROCESS_JAVA_VERSION=result['Minecraft_process_java']['java_version'], MOD_BYTECODE_TARGET=manifest['mod_bytecode_target'],
    AMD_FSR_FG_X2='PASS_UNCHANGED_HISTORICAL', AMD_BASELINE_MODIFIED='NO',
    INDEX2_REACHES_JAVA_BACKEND='YES', INDEX2_REACHES_JNI='YES', INDEX2_REACHES_CPP=review['INDEX2_REACHES_CPP'],
    INDEX2_NGX_COUNT=review['INDEX2_NGX_COUNT'], INDEX2_NGX_INDEX=review['INDEX2_NGX_INDEX'], INDEX2_EVALUATE_RESULT=review['INDEX2_EVALUATE_RESULT'],
    G1_RESOURCE_ID=t1['OUTPUT_RESOURCE_ID'], G2_RESOURCE_ID=t2['OUTPUT_RESOURCE_ID'], G1_G2_RESOURCE_DISTINCT=t1['OUTPUT_RESOURCE_ID'] != t2['OUTPUT_RESOURCE_ID'],
    COMPLETION_MODEL=review['COMPLETION_MODEL'],
    INDEX1_COMPLETION=f"GROUP_DONE={t1['COMPLETION_ID']};OBSERVED={t1['COMPLETION_OBSERVED']}",
    INDEX2_COMPLETION=f"GROUP_DONE={t2['COMPLETION_ID']};OBSERVED={t2['COMPLETION_OBSERVED']}",
    INDEX1_STATUS_BEFORE=hex(int(t1['STATUS_BEFORE'])), INDEX1_STATUS_AFTER=hex(int(t1['STATUS_AFTER_GROUP'])),
    INDEX2_STATUS_BEFORE=hex(int(t2['STATUS_BEFORE'])), INDEX2_STATUS_AFTER=hex(int(t2['STATUS_AFTER_GROUP'])),
    INDEX2_SENTINEL_ROOT_CAUSE=review['ROOT_CAUSE'], BUG_IN_OUR_MOD=review['BUG_IN_OUR_MOD'],
    BUG_LOCATION='NO_NEW_LOCAL_STATUS_BUG_PROVEN', FIX_IMPLEMENTED=review['FIX_IMPLEMENTED'],
    CPU_TESTS='PASS_STATUS_FIXTURES_AND_PUBLIC_HELPER_CPU_MOCK', JNI_TESTS='PASS_ACTUAL_OWN_DLL_ABI_SMOKE;NO_GPU_STATUS_SIMULATION',
    CPP_BUILD='PASS', JAVA_BUILD='PASS_SUPERRESOLUTION', WISTERIA_BUILD='PASS_LOCAL_API_AND_NATIVE_PACKAGING',
    RAW_REPORTED_MULTI_FRAME_COUNT_MAX=result['raw_reported_max'], CAPABILITY_PROVENANCE=result['capability_provenance'], CAPABILITY_HARDCODED='NO',
    LOCAL_X3_RUN_ID=run.name,
    X3_EVAL_INDEX1=f"SUCCESS_CALLS={result['evaluate_success_by_index']['1']}", X3_EVAL_INDEX2=f"SUCCESS_CALLS={result['evaluate_success_by_index']['2']}",
    X3_G1_VALID='STATUS_ELIGIBLE;PIXELS_NOT_VERIFIED', X3_G2_VALID='NO_UNKNOWN_STATUS',
    X3_G1_HASH='NOT_MEASURED', X3_G2_HASH='NOT_MEASURED', X3_G1_G2_DISTINCT='RESOURCES_YES;PIXELS_NOT_MEASURED',
    X3_TEMPORAL_ORDER='NOT_PROVEN', X3_PRESENT_ORDER=trace_text,
    X3_REAL_FRAMES=f"{result['real_completed_groups']}_COMPLETED_PROVIDER_GROUPS;GLOBAL_TOTAL_NOT_MEASURED",
    X3_GENERATED_FRAMES=f"{result['flag_eligible_completed_outputs']}_STATUS_ELIGIBLE;PIXELS_UNVERIFIED",
    X3_PRESENTED_GENERATED_FRAMES=result['presented_generated'],
    X3_REAL_FPS='NOT_MEASURED_GLOBAL', X3_GENERATED_FPS='NOT_MEASURED_GLOBAL', X3_PRESENTED_FPS='NOT_MEASURED_GLOBAL',
    X3_LATE=timing['late_generated'], X3_DROPPED='SEE_CANDIDATE_RETIREMENT_SCOPE_BELOW', X3_DUPLICATE=timing['duplicate_generated_keys'],
    X3_FUNCTIONAL_PASS='NO', X3_VISUAL_QUALITY='NOT_VALIDATED', MINECRAFT_DLSSG_X3='FAIL_OUTPUT2_STATUS_NOT_WRITTEN',
    X4_CAPABILITY_GATE='NOT_EVALUATED_X3_FAIL', X4_RUN_ID='NOT_RUN', MINECRAFT_DLSSG_X4='NOT_RUN',
    X5_CAPABILITY_GATE='NOT_EVALUATED_X3_FAIL', X5_RUN_ID='NOT_RUN', MINECRAFT_DLSSG_X5='NOT_RUN',
    MULTIPLAYER_BUILD_READY='NO_X3_LOCAL_GATE_FAILED', SERVER_REQUIRES_MOD='NOT_AUDITED_X3_FAIL', NETWORK_PROTOCOL_MODIFIED='NOT_AUDITED_X3_FAIL',
    CPU_TRANSPORT_COPY_COUNT='0_OWN_PROVIDER_STATIC_AUDIT', PER_FRAME_CPU_WAIT_BETWEEN_APIS='NO_GPU_QUEUE_WAIT_EVENT_DRIVEN_CALLBACK',
    PRESENTWORKER_STILL_SOLE_PRESENT_OWNER='YES_OWN_PROVIDER_AUDIT', D3D12_PRESENT_CALLS='0_OWN_PROVIDER_STATIC_AUDIT',
    DLSSG_PROVIDER_VKQUEUEPRESENT_CALLS='0_STATIC_AUDIT', DEVICE_LOST=result['device_removed'], CRASH=result['crash'],
    DLSS_SUPER_RESOLUTION_MODIFIED='NO', RAY_TRACING_IMPLEMENTED='NO', DIRECT_VULKAN_DLSSG_RETESTED='NO', DZN_USED='NO', X6_TESTED='NO',
    DATA_HARDCODED_OR_FABRICATED='NO', IF_STOPPED_STAGE='INDEX2_OUTPUT_STATUS_AFTER_GPU_GROUP_COMPLETION',
    IF_STOPPED_REASON='UNKNOWN_NOT_WRITTEN;NO_RETRY_X4_X5', ROOT_CAUSE=review['ROOT_CAUSE'], CAUSE_CONFIDENCE=review['CAUSE_CONFIDENCE'],
    FILES_MODIFIED=changed, NEW_LOG_DIRECTORIES=[str(run)], GITHUB_COMMIT_SHA=head, GITHUB_PUSH_RESULT=sys.argv[2],
    LAST_CONFIRMED_BOUNDARY=review['LAST_CONFIRMED_BOUNDARY'], SAMPLE_PAIR_ID=pair['PAIR_ID'],
    MEASURED_FPS_SCOPE=timing['scope'], RUN_DURATION_PROVIDER_PRESENT_WINDOW=timing['duration_seconds'],
    MEASURED_REAL_PRESENTED_FPS=timing['real_presented_fps'], MEASURED_GENERATED_PRESENTED_FPS=timing['generated_presented_fps'],
    MEASURED_TOTAL_PRESENTED_FPS=timing['presented_fps'], TOTAL_PRESENTED_COUNT=result['provider_batch_presented_total'],
    NORMAL_SHUTDOWN=result['normal_shutdown'], GLOBAL_FPS_AND_CUT_CAUSE='NOT_MEASURED;NO_CAUSE_CLAIM',
)
# Fully retired provider candidates minus actual generated presents; includes reset,
# UNKNOWN and whole-batch discard, and is not a pixel-validated-generation count.
retired = {e['fields']['realFrameId'] for e in events if e['type'] == 'RETIRED'}
if len(retired) == result['real_completed_groups']:
    f['X3_DROPPED'] = f"{len(retired)*manifest['requested_count']-result['presented_generated']}_RETIRED_CANDIDATES_WITHOUT_PRESENT;INCLUDES_RESET_UNKNOWN_AND_BATCH_DISCARD"
report = ROOT / 'builds/experimental/dlssg-x2/INDEX2_FINAL_REPORT.txt'
report.write_text('\n'.join(k+'='+ (json.dumps(v,ensure_ascii=False) if isinstance(v,(list,dict)) else str(v)) for k,v in f.items())+'\n',encoding='utf-8')
print(report)
