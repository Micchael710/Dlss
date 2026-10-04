"""Record actual results of the sole SDK 2.12 run; never substitute proxy ceilings."""
from pathlib import Path
import hashlib,json,re
root=Path(__file__).resolve().parents[2]
r=root/'logs/research/streamline-mfg-capability/20261004-sdk212'
log=(r/'harness.log').read_text(encoding='utf-8-sig',errors='replace')
sl=(r/'sl.log').read_text(encoding='utf-8-sig',errors='replace')
values={}
for line in log.splitlines():
    if '=' in line and not line.startswith('LOADED_MODULE='):
        key,value=line.split('=',1);values[key]=value
staged=json.loads((r/'staged-sdk.json').read_text())
process=json.loads((r/'process-result.json').read_text(encoding='utf-8-sig'))
used=[]
runtime=Path(staged['runtime'])
for file in staged['files']:
    path=runtime/file['name']
    loaded=str(path) in log or file['name'].removesuffix('.dll') in re.findall(r"Loaded plugin '([^']+)'",sl)
    if loaded:
        actual=hashlib.sha256(path.read_bytes()).hexdigest();assert actual==file['sha256']
        used.append(dict(file,observed_loaded=True))
version=re.search(r'Streamline v([^ ]+)',sl)
architecture=re.search(r'Adapter 0 architecture ([^ ]+)',sl)
support=int(values['DLSSG_FEATURE_SUPPORTED_RESULT']) if 'DLSSG_FEATURE_SUPPORTED_RESULT' in values else None
max_value=int(values['NUM_FRAMES_TO_GENERATE_MAX']) if 'NUM_FRAMES_TO_GENERATE_MAX' in values else None
result=dict(status='BLOCKED_FEATURE_SUPPORT' if support==6 else 'OBSERVED_RESULT',
    github_start_head='a136e68a9fc7fcef2443ae9c95e25df706389188',streamline_package='2.12.0',
    runtime_version=version[1] if version else 'NOT_OBSERVED',gpu_name=values.get('GPU_NAME','NOT_OBSERVED'),
    vendor_id=values.get('VENDOR_ID'),device_id=values.get('DEVICE_ID'),adapter_luid=values.get('ADAPTER_LUID'),
    historical_luid_matches=values.get('ADAPTER_LUID')=='4c29010000000000',
    sm86_component_version='0.3.5',sm86_proxy_loaded=values.get('SM86_PROXY_LOADED'),
    sm86_proxy_active='NOT_DEMONSTRATED_FOR_STREAMLINE',streamline_reported_architecture=architecture[1] if architecture else None,
    sm86_config_max_generated_frames=staged['config_max_generated_frames'],
    sm86_config_spoof_arch_to_game=0,config_ceiling_not_runtime_capability=True,
    streamline_init_result=values.get('STREAMLINE_INIT_RESULT','NOT_CALLED'),
    feature_support_result=support,feature_support_result_name='eErrorNoSupportedAdapterFound' if support==6 else 'SEE_SL_RESULT_HEADER',
    dlssg_feature_supported=support==0 if support is not None else 'NOT_QUERIED',
    dlssg_get_state_result=values.get('DLSSG_GET_STATE_RESULT','NOT_CALLED'),dlssg_status=values.get('DLSSG_STATUS','NOT_QUERIED'),
    num_frames_to_generate_max=max_value,max_value_source='NONE_GETSTATE_NOT_REACHED' if max_value is None else 'PUBLIC_GETSTATE_WITH_LOADED_PROXY_ACTIVE_UNPROVEN',
    set_options={str(n):values.get(f'SET_OPTIONS_{n}_RESULT','NOT_CALLED') for n in range(1,5)},
    capability={f'x{n+1}':'UNKNOWN' if max_value is None else max_value>=n for n in range(1,5)},
    process_exit_code=process['process_exit_code'],streamline_shutdown_result=values.get('STREAMLINE_SHUTDOWN_RESULT'),
    if_fail_stage=values.get('FAIL_STAGE','UNKNOWN'),
    root_cause='Public feature query rejected adapter; SL DLSS-G plugin hardware mask 0x0 and unloaded as unsupported. Why the loaded SM86 proxy did not establish support is UNKNOWN.',
    cause_confidence='HIGH_FOR_FEATURE_GATE;SM86_UNDERLYING_CAUSE_UNKNOWN',
    attempts=process['attempts'],loaded_staged_dll_hashes=used,
    minecraft_executed=False,present_calls=0,generated_frames_tested=False,
    x2_baseline_modified=False,presentworker_modified=False,amd_baseline_modified=False,
    cpu_frame_transport_introduced=False,data_hardcoded_or_fabricated=False)
(r/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['status','runtime_version','gpu_name','feature_support_result','num_frames_to_generate_max','process_exit_code']},indent=2))
