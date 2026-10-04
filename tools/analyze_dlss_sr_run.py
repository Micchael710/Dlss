"""Summarize actual SR events and bounded image readbacks; never infers a runtime PASS."""
from pathlib import Path
import sys,json,collections,hashlib
import numpy as np
from PIL import Image
r=Path(sys.argv[1]); events=[json.loads(s) for s in (r/'dlss-sr-events.jsonl').read_text().splitlines()]
counts=collections.Counter(e['type'] for e in events)
summary={'run_id':r.name,'counts':dict(counts),'first_gates':[e for e in events if e['type'] in ('RUNTIME','VULKAN_INIT_RESULT','VULKAN_PREREQUISITE','GL_VK_INTEROP_READY','NATIVE_LOAD','NGX_INIT','FEATURE_REQUIREMENTS','CAPABILITY','CREATE_INPUTS','CREATE_FEATURE','CREATE_COMPLETION')],
         'last_evaluate':next((e for e in reversed(events) if e['type']=='EVALUATE'),None),
         'shutdown':[e for e in events if e['type'] in ('SHUTDOWN_BEGIN','NGX_SHUTDOWN','FEATURES_RETIRED','GRAPHICS_BACKEND_DESTROYED')], 'samples':[]}
samples=[e for e in events if 'file' in e and 'readback' in e]
for out in [e for e in samples if e['file'].endswith('-OutputColor.bin')]:
    inp=next(e for e in samples if e['evaluation']==out['evaluation'] and e['file'].endswith('-Color.bin'))
    arrays=[]
    for e in (inp,out):
        b=(r/e['file']).read_bytes(); a=np.frombuffer(b,dtype=np.uint8).reshape(int(e['height']),int(e['width']),4); arrays.append(a)
    color,output=arrays
    nearest=np.asarray(Image.fromarray(color).resize((output.shape[1],output.shape[0]),Image.Resampling.NEAREST))
    summary['samples'].append({'evaluation':out['evaluation'],'input_size':[color.shape[1],color.shape[0]],'output_size':[output.shape[1],output.shape[0]],
        'output_sha256':hashlib.sha256(output.tobytes()).hexdigest(),'black_fraction':float(np.all(output[:,:,:3]==0,axis=2).mean()),
        'output_mean_rgb':float(output[:,:,:3].mean()),'output_vs_nearest_input_mae':float(np.abs(output[:,:,:3].astype(float)-nearest[:,:,:3]).mean())})
    Image.fromarray(output[::-1]).save(r/('sr-output-'+out['evaluation']+'.png'))
(r/'sr-observation-summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print(json.dumps({'counts':summary['counts'],'samples':summary['samples'],'shutdown':summary['shutdown']},indent=2))
