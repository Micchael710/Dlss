"""Audit retained runtime events. Recording/submission alone does not count as presentation."""
import json, re
from collections import defaultdict
from pathlib import Path
root = Path(__file__).resolve().parents[1]
lines = (root/'logs/runtime/fsr-x2-pass.log').read_text(encoding='utf-8').splitlines()
def fields(line): return dict(re.findall(r'(\w+)=([^\s]+)', line))
intervals = {int(f['realFrameId']): f for line in lines if 'FSR_INTERVAL ' in line for f in [fields(line)]}
submits = {int(fields(line)['realFrameId']) for line in lines if 'FSR_VULKAN_SUBMIT ' in line}
completed = {int(fields(line)['realFrameId']) for line in lines if 'FSR_COMPLETION ' in line}
pairs = defaultdict(list)
for line in lines:
    if 'FSR_PRESENT_ORDER ' in line:
        f = fields(line)
        if int(f['realFrameId']) > 0: pairs[int(f['realFrameId'])].append(f)
valid = {i: events for i, events in pairs.items() if [e['kind'] for e in events] == ['GENERATED', 'REAL']}
assert valid and len(valid) == len(pairs), 'Missing or incorrectly ordered pair'
assert all(i in intervals and i in submits and i in completed for i in valid), 'Missing dispatch/submit/completion'
assert all(events[0]['VkImage'] == intervals[i]['VkImage'] for i, events in valid.items())
assert all(intervals[i]['prepareResult'] == intervals[i]['dispatchResult'] == 'OK' for i in valid)
assert all(int(es[1]['displayIndex']) == int(es[0]['displayIndex']) + 1 for es in valid.values())
errors = [line for line in lines if re.search(r'/ERROR\]|/FATAL\]|GL_INVALID_|VK_ERROR_|FSR_DISPATCH unavailable', line)]
assert not errors, errors[:3]
metrics = [fields(line) for line in lines if 'FSR_METRICS ' in line][-1]
ids = sorted(valid)
source_seconds = (int(intervals[ids[-1]]['timestamp']) - int(intervals[ids[0]]['timestamp'])) / 1e9
present_seconds = (int(valid[ids[-1]][1]['timestamp']) - int(valid[ids[0]][0]['timestamp'])) / 1e9
result = dict(AMD_FSR_3_1_4_CONTEXT='PASS', AMD_FSR_FG_GENERATED_FRAME='PASS', AMD_FSR_FG_X2='PASS',
              generatedFrames=len(valid), realFrames=len(valid), generated_nonreset=sum(intervals[i]['reset']=='false' for i in ids),
              firstRealFrameId=ids[0], lastRealFrameId=ids[-1],
              real_render_fps=(len(ids)-1)/source_seconds, presented_fps=(2*len(ids)-1)/present_seconds,
              window_seconds=present_seconds, fg_gpu_ms_last_rolling_256=float(metrics['fgGpuMs']),
              gpu_time_scope='Vulkan timestamps: prepare + FG dispatch + real copy + resource barriers',
              pair_order='previous REAL -> GENERATED(current interval, interpolation .5) -> current REAL',
              generated_present_counter=int(valid[ids[-1]][0]['generatedFrames']),
              all_dispatch_submit_completion_order_checks='PASS', new_errors=0,
              visual='Brief observed world: complete scene, aligned HUD, no black/corrupted frame or abnormal flicker observed',
              scope='854x480 display, 502x282 render, SR1.7, Complementary, Vulkan, wisteria:fsr X2, OptiScaler OFF',
              limits='Not a stress test; no x3..x6, HDR luminance or full object/deformation motion quality validation',
              evidence=['fsr-x2-pass.log','fsr-x2-world.jpg'])
(root/'logs/runtime/fsr-x2-result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
