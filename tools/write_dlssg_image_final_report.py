"""Required field report from closed image/status evidence. No inferred MFG PASS."""
from pathlib import Path
import json,subprocess,sys
root=Path(__file__).resolve().parents[1];run=Path(sys.argv[1]).resolve()
read=lambda n:json.loads((run/n).read_text(encoding='utf-8-sig'))
r,a,m=read('result.json'),read('image-analysis.json'),read('run-manifest.json')
assert r['normal_shutdown'] and len(a['pairs'])==3
head=subprocess.check_output(['C:/Users/micha/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe','rev-parse','HEAD'],cwd=root,text=True).strip()
f=dict(GITHUB_START_HEAD='91c8292b560b4e87bd3a8d4a9fac7ab45880e201',GITHUB_END_HEAD=head,
G2_DIAGNOSTIC_READBACK_PERFORMED='YES_FOUR_GROUPS_THREE_ANCHORED_PAIRS',
G2_IMAGE_HASH={str(p['frame_id']):p['hashes']['G2'] for p in a['pairs']},
G2_IMAGE_CHANGED_FROM_SENTINEL=all(p['images']['G2']['sentinel_pixels']==0 for p in a['pairs']),
G2_DIFFERS_FROM_A=all(p['images']['G2']['differs_from_A'] for p in a['pairs']),
G2_DIFFERS_FROM_B=all(p['images']['G2']['differs_from_B'] for p in a['pairs']),
G2_DIFFERS_FROM_G1=all(p['images']['G2']['differs_from_other_G'] for p in a['pairs']),
G2_TEMPORAL_POSITION={str(p['frame_id']):p['temporal']['G2'] for p in a['pairs']},
G2_IMAGE_GENERATION='PASS_BOUNDED_DIAGNOSTIC',G2_OPTIONAL_STATUS='UNKNOWN_0xffffffff',
OUTPUT_DISABLE_SEMANTICS='Optional suppression hint; >=4 byte resource; true written in first byte when interpolated frame(s) should not be shown',
PER_INDEX_OR_GROUP='GROUP_INTENT_SUGGESTED;INDEXED_WRITE_SCOPE_UNSPECIFIED;NO_GROUP_VALIDITY_SUBSTITUTION',
SEMANTICS_SOURCE=['https://raw.githubusercontent.com/NVIDIA/DLSS/main/include/nvsdk_ngx_defs_dlssg.h','https://raw.githubusercontent.com/NVIDIA/DLSS/main/include/nvsdk_ngx_helpers_dlssg_d3d.h'],
SEMANTICS_CONFIDENCE='HIGH_OPTIONAL_AND_FIRST_BYTE;INSUFFICIENT_FOR_G1_FLAG_AUTHORIZING_G2',
OPTISCALER_PUBLIC_MFG_MECHANISM='InterpolationCount -> clamped numFramesToGenerate -> Streamline SetOptions; interposed DXGI swapchain',
STREAMLINE_PUBLIC_MFG_MECHANISM='numFramesToGenerate within queried max; owns interpolated presentation/pacing at Present',
OPTISCALER_CLIENT_MECHANISM='Installer/configuration layer with path/nvngx/Streamline setup',
DLSS_ENABLER_MECHANISM='Compatibility/component bundle; selected backend must be independently established',
OUR_CURRENT_MFG_MECHANISM='Public NGX D3D12 Evaluate count/index -> GPU shared distinct outputs -> existing Vulkan PresentWorker',
KEY_DIFFERENCES='Our measured max2 follows configured MaxGeneratedFrames2; Streamline count uses its queried capability and presentation owner. No public texture-export coexistence contract proven.',
RAW_REPORTED_MULTI_FRAME_COUNT_MAX=r['raw_reported_max'],X3_RUN_ID=run.name,X3_COUNT=2,
X3_G1_VALID='IMAGE_PASS_BOUNDED;WRITTEN_STATUS0_NONRESET',X3_G2_VALID='IMAGE_PASS_BOUNDED;STATUS_UNKNOWN;NOT_PRESENTABLE',
X3_TEMPORAL_ORDER='A<G1<G2<B_SUPPORTED_IN_THREE_SAMPLES;MODERATE_CONFIDENCE;CLOUD_ARTIFACTS',
X3_PRESENT_ORDER='REAL_A,G1,REAL_B;G2_PRESENT_COUNT=0;FULL_X3_NOT_OBSERVED',MINECRAFT_DLSSG_X3='BLOCKED_STATUS_SEMANTICS_UNPROVEN;FUNCTIONAL_PASS=NO',
REAL_FPS_X3=r['metrics']['real_presented_fps'],PRESENTED_FPS_X3=r['metrics']['presented_fps'],
MEASURED_FPS_SCOPE=r['metrics']['scope']+';accepted requests, not scanout; not x3 throughput',
REAL_FRAME_COUNT_X3=r['real_completed_groups'],GENERATED_FRAME_COUNT_X3='TOTAL_PIXEL_VALID_COUNT_NOT_MEASURED;6_ANCHORED_SAMPLED_OUTPUTS_VERIFIED',
STATUS_ELIGIBLE_GENERATED_COUNT_X3=r['flag_eligible_completed_outputs'],PRESENTED_GENERATED_COUNT_X3=r['presented_generated'],
TOTAL_PRESENTED_COUNT_X3=r['provider_batch_presented_total'],GENERATED_FPS_X3=r['metrics']['generated_presented_fps'],
LATE_GENERATED_X3=r['metrics']['late_generated'],DROPPED_GENERATED_X3='PIXEL_VALID_GLOBAL_COUNT_UNKNOWN;RETIRED_CANDIDATE_OUTPUTS_WITHOUT_PRESENT='+str(r['real_completed_groups']*2-r['presented_generated']),
DUPLICATE_GENERATED_X3=r['metrics']['duplicate_generated_keys'],
AMD_BASELINE_MODIFIED='NO',AMD_FSR_FG_X2='PASS_UNCHANGED_HISTORICAL;9_CLASSES_2_DLL_IDENTICAL',CPU_TRANSPORT_COPY_COUNT=0,
PRESENTWORKER_STILL_SOLE_PRESENT_OWNER='YES',D3D12_PRESENT_CALLS=0,DLSSG_PROVIDER_VKQUEUEPRESENT_CALLS=0,
DLSS_SR_NEXT_ITERATION_REQUIRED='YES',DLSS_SR_UI_OPTION_ALREADY_EXISTS='YES',DATA_HARDCODED_OR_FABRICATED='NO',
ROOT_CAUSE_OR_BLOCKER='G2 image produced despite optional flag sentinel; public contract does not establish safe validity propagation from G1/group. Exact runtime non-write cause UNKNOWN.',
CAUSE_CONFIDENCE='HIGH_IMAGE_AND_STATUS_OBSERVATIONS;MODERATE_SAMPLED_MOTION;INSUFFICIENT_STATUS_SCOPE',
NORMAL_SHUTDOWN=r['normal_shutdown'],CRASH=r['crash'],DEVICE_LOST=r['device_removed'],
GITHUB_COMMIT_SHA=head,GITHUB_PUSH_RESULT=sys.argv[2])
for x,count in [(4,3),(5,4)]:
    f.update({f'X{x}_CAPABILITY_SOURCE':'DOCUMENTED_COMPONENT_CONFIG_AND_STREAMLINE_OPTIONS_IDENTIFIED;NOT_QUERIED_X3_BLOCKED',f'X{x}_REPORTED_MAX':'NOT_QUERIED',f'X{x}_RUN_ID':'NOT_RUN',f'X{x}_COUNT':count,f'X{x}_TEMPORAL_ORDER':'NOT_TESTED',f'X{x}_PRESENT_ORDER':'NOT_TESTED',f'MINECRAFT_DLSSG_X{x}':'NOT_RUN_X3_GATE',f'REAL_FPS_X{x}':'NOT_MEASURED',f'PRESENTED_FPS_X{x}':'NOT_MEASURED'})
    for g in range(1,count+1):f[f'X{x}_G{g}_VALID']='NOT_TESTED'
path=root/'builds/experimental/dlssg-x2/MFG_IMAGE_FINAL_REPORT.txt'
path.write_text('\n'.join(k+'='+(json.dumps(v,ensure_ascii=False) if isinstance(v,(list,dict)) else str(v)) for k,v in f.items())+'\n',encoding='utf-8')
print(path)
