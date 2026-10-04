"""Offline bounded image observation; never authorizes presentation or changes flags."""
from pathlib import Path
import hashlib,json,re,sys
import numpy as np
import cv2
from PIL import Image,ImageDraw

def fields(s):return dict(re.findall(r'(\w+)=([^\s;]+)',s))
def temporal(a,b,images):
    # Camera motion projection on textured central scenery, excluding HUD and borders.
    gray=lambda x:cv2.cvtColor(cv2.resize(x[:,:,:3],(640,360)),cv2.COLOR_RGB2GRAY)
    ag,bg=gray(a),gray(b)
    flow=lambda x:cv2.calcOpticalFlowFarneback(ag,x,None,.5,4,25,5,7,1.5,0)
    ab=flow(bg);mag=np.linalg.norm(ab,axis=2)
    texture=np.hypot(cv2.Sobel(ag,cv2.CV_32F,1,0),cv2.Sobel(ag,cv2.CV_32F,0,1))
    mask=np.zeros(ag.shape,bool);mask[36:288,64:576]=True
    mask &= (mag>1)&(mag<60)&(texture>15)
    result={'method':'Farneback A->Gi motion projected onto A->B; central textured scenery; diagnostic estimate, not ground truth','motion_pixels':int(mask.sum())}
    for name,g in images.items():
        agi=flow(gray(g));tau=np.sum(agi*ab,axis=2)/(mag*mag+1e-6)
        residual=np.linalg.norm(agi-tau[:,:,None]*ab,axis=2)/(mag+1e-6)
        good=mask & (residual<.3)
        result[name]={'support_pixels':int(good.sum()),'tau_median':float(np.median(tau[good])) if good.any() else None,'tau_quartiles':np.quantile(tau[good],[.25,.75]).tolist() if good.any() else [],'median_relative_off_axis_error':float(np.median(residual[mask])) if mask.any() else None}
    t1,t2=result['G1']['tau_median'],result['G2']['tau_median']
    result['ordered_motion_estimate']=bool(t1 is not None and t2 is not None and 0<t1<t2<1 and min(result[k]['support_pixels'] for k in images)>1000)
    return result

def analyze(out):
    assert not (out/'image-analysis.json').exists(),'Preserve closed analysis'
    records=[json.loads(x) for x in (out/'provider-events.jsonl').read_text(encoding='utf-8-sig').splitlines() if x.strip()]
    meta={int(f['realFrameId']):f for e in records if e['type']=='sample_metadata' for f in [fields(e['detail'])]}
    statuses={(int(f['realFrameId']),int(f['index'])):f['disable'] for e in records if e['type']=='output_sample' for f in [fields(e['detail'])]}
    sequence=[json.loads(x) for x in (out/'frame-sequence.jsonl').read_text(encoding='utf-8-sig').splitlines() if x.strip()]
    prepared={int(f['realFrameId']):f for e in sequence if e['type']=='PREPARED' for f in [fields(e['detail'])]}
    evaluated={(int(f['realFrameId']),int(f['index'])):f for e in records if e['type']=='evaluate' for f in [fields(e['detail'])]}
    completed={(int(f['realFrameId']),int(f['index'])):f for e in records if e['type']=='completion' for f in [fields(e['detail'])]}
    result={'diagnostic_only':True,'presentation_gating_changed':False,'pairs':[]}
    for frame,f in sorted(meta.items()):
        w,h=int(f['width']),int(f['height'])
        if frame-1 not in meta or meta[frame-1]['width']!=f['width'] or meta[frame-1]['height']!=f['height']:continue
        assert prepared[frame]['reset']=='false' and prepared[frame-1]['epoch']==prepared[frame]['epoch'],'A/B crosses history discontinuity'
        for index in [1,2]:
            key=frame,index
            assert evaluated[key]['result']=='0x00000001' and evaluated[key]['done']==completed[key]['done'],'Missing successful evaluation/completion'
        paths={k:out/f'sample-{frame if k!="A" else frame-1}-{k if k!="A" else "B"}.bin' for k in ['A','G1','G2','B']}
        data={k:p.read_bytes() for k,p in paths.items()}
        images={k:np.frombuffer(v,np.uint8).reshape(h,w,4) for k,v in data.items()}
        hashes={k:hashlib.sha256(v).hexdigest() for k,v in data.items()}
        row={'frame_id':frame,'hashes':hashes,'images':{}}
        for k in ['G1','G2']:
            g=images[k][:,:,:3];sent=np.all(g==[255,0,255],axis=2);black=np.all(g==0,axis=2)
            row['images'][k]={'optional_status':statuses[frame,int(k[1])],'sentinel_pixels':int(sent.sum()),'black_fraction':float(black.mean()),'differs_from_A':hashes[k]!=hashes['A'],'differs_from_B':hashes[k]!=hashes['B'],'differs_from_other_G':hashes[k]!=hashes['G2' if k=='G1' else 'G1'],'nonblack_nonsentinel_distinct':bool(not sent.any() and black.mean()<.99 and all(hashes[k]!=hashes[j] for j in hashes if j!=k))}
        row['temporal']=temporal(images['A'],images['B'],{k:images[k] for k in ['G1','G2']})
        tiles=[Image.fromarray(images[k][:,:,:3]).resize((640,360)) for k in ['A','G1','G2','B']]
        sheet=Image.new('RGB',(1280,760));draw=ImageDraw.Draw(sheet)
        for i,(k,tile) in enumerate(zip(['A','G1','G2','B'],tiles)):
            x,y=(i%2)*640,(i//2)*380;sheet.paste(tile,(x,y+20));draw.text((x+8,y+4),f'{k} pair {frame} diagnostic',fill='white')
        sheet.save(out/f'contact-{frame}.png')
        result['pairs'].append(row)
    (out/'image-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':analyze(Path(sys.argv[1]).resolve())
