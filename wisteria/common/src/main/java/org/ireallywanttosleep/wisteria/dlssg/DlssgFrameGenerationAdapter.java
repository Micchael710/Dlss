package org.ireallywanttosleep.wisteria.dlssg;

import io.homo.superresolution.api.registry.framegeneration.*;
import io.homo.superresolution.common.config.SuperResolutionConfig;
import io.homo.superresolution.common.presentation.vulkan.AsyncFramePresenter;
import io.homo.superresolution.core.graphics.impl.texture.*;
import io.homo.superresolution.core.graphics.vulkan.*;
import org.ireallywanttosleep.wisteria.Wisteria;
import org.lwjgl.vulkan.VK;
import java.util.*;
import java.nio.file.*;
import java.io.*;
import static org.lwjgl.vulkan.VK10.*;

final class DlssgFrameGenerationAdapter {
    private final boolean shadow=!"presentation".equals(System.getProperty("wisteria.dlssg.mode","shadow"));
    private volatile boolean healthy=true, invalidHistory=true;
    private Thread owner;
    private long session,lastId=-1,lastEpoch=-1;
    private Pool current;
    private final List<Pool> pools=new ArrayList<>();
    private PrintWriter events;
    private PrintWriter jsonEvents;
    private int dispatched,samples;
    private final DlssgDiagnosticCapture diagnosticCapture=new DlssgDiagnosticCapture(Boolean.getBoolean("wisteria.dlssg.imageDiagnostic"));
    private volatile int reportedMax;
    private final int requestedMax=Integer.getInteger("wisteria.dlssg.requestedCount",1);
    int supportedCount(){return Math.min(reportedMax,Math.min(requestedMax,4));}
    void bootstrap(VulkanDevice device){initialize(device);}
    boolean healthy(){return healthy;}
    void invalidateHistory(){invalidHistory=true;}
    void fail(String stage,Throwable error){
        healthy=false;invalidHistory=true;
        Wisteria.LOGGER.error("DLSSG_EXPERIMENT_FAIL stage={} (no retry)",stage,error);
        try{Path out=Path.of(System.getProperty("wisteria.dlssg.runDir"));Files.createDirectories(out);
            Files.writeString(out.resolve("integration-failure.txt"),"stage="+stage+"\n"+error+"\n");
        }catch(Exception ignored){}
    }
    private void requireOwner(){if(owner==null)owner=Thread.currentThread();if(owner!=Thread.currentThread())throw new IllegalStateException("DLSS-G accessed outside FG worker");}
    private synchronized void event(String message){if(events!=null){
        long now=System.nanoTime();events.println(now+" "+message);
        var record=new LinkedHashMap<String,Object>();record.put("timestamp_ns",now);
        record.put("type",message.split(" ",2)[0]);record.put("detail",message);
        var fields=new LinkedHashMap<String,String>();
        var matcher=java.util.regex.Pattern.compile("(\\w+)=([^\\s;]+)").matcher(message);
        while(matcher.find())fields.put(matcher.group(1),matcher.group(2));record.put("fields",fields);
        if(jsonEvents!=null)jsonEvents.println(new com.google.gson.Gson().toJson(record));
    }}
    private void initialize(VulkanDevice device){
        if(session!=0)return;
        try{Path out=Path.of(System.getProperty("wisteria.dlssg.runDir")).toAbsolutePath();
            Files.createDirectories(out);
            var runtime=new LinkedHashMap<String,Object>();
            runtime.put("java_version",System.getProperty("java.version"));runtime.put("java_home",System.getProperty("java.home"));
            runtime.put("java_runtime_version",System.getProperty("java.runtime.version"));runtime.put("pid",ProcessHandle.current().pid());
            runtime.put("process_command",ProcessHandle.current().info().command().orElse("UNKNOWN"));
            Files.writeString(out.resolve("runtime-jvm.json"),new com.google.gson.GsonBuilder().setPrettyPrinting().create().toJson(runtime));
            if(!"25.0.4".equals(System.getProperty("java.version")))throw new IllegalStateException("Controlled experiment requires Minecraft Java25.0.4");
            events=new PrintWriter(Files.newBufferedWriter(out.resolve("frame-sequence.log")),true);
            jsonEvents=new PrintWriter(Files.newBufferedWriter(out.resolve("frame-sequence.jsonl")),true);
            session=DlssgBridge.create(device.getVkInstance().address(),device.getPhysicalDevice().address(),device.getVkDevice().address(),
                device.requireFgQueue().getQueue().address(),device.requireFgQueue().getQueueFamilyIndex(),
                VK.getFunctionProvider().getFunctionAddress("vkGetInstanceProcAddr"),out.toString(),
                System.getProperty("wisteria.dlssg.externalDll"),System.getProperty("wisteria.dlssg.runtimeDir"),requestedMax);
            reportedMax=DlssgBridge.reportedMax(session);
            if(Boolean.getBoolean("sr.dlss.secondSubmitDiagnostics"))
                DlssgBridge.configureDiagnostics(session,out.toString(),device.getVkDevice().getCapabilities().VK_EXT_device_fault,
                        device.getVkDevice().getCapabilities().VK_NV_device_diagnostic_checkpoints,
                        device.getVkDevice().getCapabilities().VK_KHR_synchronization2);
            if(requestedMax<1||requestedMax>4||reportedMax<requestedMax)throw new IllegalStateException("Requested MFG count not permitted by observed NGX capability");
            event("SESSION persistent=true shadow="+shadow+" requestedCount="+requestedMax+" reportedMax="+reportedMax+" outputPoolSlots="+AsyncFramePresenter.maximumLiveProviderLeases(requestedMax));
        }catch(Exception e){throw new IllegalStateException("DLSS-G session initialization failed",e);}
    }
    FrameGenerationDispatchResult dispatch(FrameGenerationDispatchInput input){
        int shortLimit=Integer.getInteger("wisteria.dlssg.shortRunLimit",0);
        if(shortLimit>0&&dispatched>=shortLimit){if(healthy)event("SHORT_RUN_COMPLETE dispatches="+dispatched);healthy=false;}
        requireOwner();if(diagnosticCapture.limitReached(dispatched)){healthy=false;event("DIAGNOSTIC_LIMIT dispatches="+dispatched+" presentationValidityUnchanged=true");}
        if(!healthy)return FrameGenerationDispatchResult.failed("DLSS-G experiment latched unavailable; no retry");
        Slot slot=null;
        String stage="INPUT_MAPPING";
        try{
            var snapshot=(DlssgFrameGenerationBackend.Snapshot)input.providerInputSnapshot();var metadata=Objects.requireNonNull(snapshot.metadata());
            var frame=input.frameResources();
            // Retain the rejection unless a fresh receipt binds these exact images to
            // a successful SR submission and its binary signals on the same family.
            var borrowed=frame.hasBorrowedAlgorithmInputs()?frame.claimBorrowedReadiness(input.device()):null;
            VulkanTexture[] sources={frame.finalColorVulkanTexture(),frame.hudlessColorVulkanTexture(),frame.depthVulkanTexture(),frame.motionVectorVulkanTexture()};
            for(var src:sources)if(src==null||src.handle()==0)throw new IllegalStateException("Missing real FG input");
            int w=metadata.displaySize().width(),h=metadata.displaySize().height(),rw=metadata.renderSize().width(),rh=metadata.renderSize().height();
            if(w!=input.outputWidth()||h!=input.outputHeight()){
                invalidHistory=true;
                return FrameGenerationDispatchResult.failed("Resize drain: snapshot and swapchain extents differ");
            }
            for(var src:sources)if(!src.getTextureUsages().getUsages().contains(TextureUsage.TransferSource))
                throw new IllegalStateException("Captured input lacks Vulkan transfer-source usage");
            if(!"SRGB".equalsIgnoreCase(metadata.color().transferFunction()))throw new IllegalStateException("First integration requires measured SDR/SRGB color metadata");
            if(sources[2].getTextureFormat()!=TextureFormat.R32F)throw new IllegalStateException("Depth must be captured R32F normalized device depth");
            // Owned capture resources always flip Y. Borrowed resources use the configured algorithm convention.
            boolean flip=snapshot.flipBorrowedInputs();
            float[] constants=DlssgMetadata.pack(snapshot.constants(),rw,rh,flip);
            if(snapshot.constants().motionVectorsJittered()!=0)
                Wisteria.LOGGER.debug("DLSSG quality limitation: producer marks motion jittered; no private NGX flag or fabricated previous jitter supplied");
            stage="SESSION_INIT";initialize(input.device());
            int count=snapshot.mode().generatedFrameCount();
            if(count<1||count>supportedCount())throw new IllegalStateException("Requested count exceeds live capability");
            Key key=new Key(w,h,rw,rh,count,sources[0].getTextureFormat().vk(),sources[1].getTextureFormat().vk(),sources[3].getTextureFormat().vk(),flip);
            if(current==null||!current.key.equals(key)){
                stage="POOL_CREATE_FEATURE";if(current!=null)current.retired=true;
                current=new Pool(input.device(),key);pools.add(current);invalidHistory=true;
                event("POOL_CREATE key="+key+" persistentHandles=true");
            }
            collectRetiredPools();slot=current.acquire();stage="PREPARE_EVALUATE";
            boolean reset=invalidHistory||snapshot.historyResetRequested()||metadata.discontinuityEpoch()!=lastEpoch||metadata.monotonicFrameId()!=lastId+1;
            long[] nativeSources=new long[borrowed==null?20:32];for(int j=0;j<4;j++){
                var src=sources[j];int expectedW=j<2?w:rw,expectedH=j<2?h:rh;
                if(src.getWidth()!=expectedW||src.getHeight()!=expectedH)throw new IllegalStateException("Resource/metadata extent mismatch");
                int off=j*5;nativeSources[off]=src.handle();nativeSources[off+1]=src.getWidth();nativeSources[off+2]=src.getHeight();nativeSources[off+3]=src.getTextureFormat().vk();nativeSources[off+4]=src.getCurrentLayout();
            }
            if(borrowed!=null){var p=borrowed.producer();
                long[] receipt={p.device(),p.queue(),p.family(),p.index(),p.command(),p.generation(),p.fence(),
                    borrowed.captureGeneration(),borrowed.frame(),borrowed.depth().ready(),borrowed.motion().ready(),1};
                System.arraycopy(receipt,0,nativeSources,20,receipt.length);
                long[] depthDest=DlssgBridge.image(current.nativePool,slot.index,2),motionDest=DlssgBridge.image(current.nativePool,slot.index,3);
                borrowed.requireIndependentDestination(depthDest[0],motionDest[0],sources[2].getCurrentLayout(),sources[3].getCurrentLayout());
                event("BORROWED_QUEUE_JOIN realFrameId="+metadata.monotonicFrameId()+" logicalFrame="+borrowed.frame()+" captureGeneration="+borrowed.captureGeneration()+
                    " producer="+p+" depth="+borrowed.depth()+" motion="+borrowed.motion()+" consumerFamily="+input.device().requireFgQueue().getQueueFamilyIndex()+
                    " consumerIndex="+input.device().requireFgQueue().getQueueIndex()+" depthDestination="+depthDest[0]+" motionDestination="+motionDest[0]+" cpuWait=false");
            }
            float[] previousClip=snapshot.constants().clipToPrevClip();double cameraMotion=0;
            for(int j=0;j<16;j++)cameraMotion=Math.max(cameraMotion,Math.abs(previousClip[j]-(j%5==0?1:0)));
            // First bounded sample requires actual camera motion; following samples anchor A/B.
            boolean sample=diagnosticCapture.enabled()?diagnosticCapture.select(dispatched,reset,cameraMotion)
                :!reset&&dispatched>=30&&samples<3&&slot.index<3&&(samples>0||cameraMotion>0.01);
            if(sample){samples++;event("SAMPLE_REQUEST realFrameId="+metadata.monotonicFrameId()+" cameraMatrixDelta="+cameraMotion);}
            long[] wait=DlssgBridge.prepare(session,current.nativePool,slot.index,input.commandBuffer(),nativeSources,constants,
                metadata.monotonicFrameId(),metadata.realFrameDeltaMs(),reset,flip,sample);
            slot.real.setCurrentLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);for(var image:slot.generated)image.setCurrentLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            invalidHistory=false;lastId=metadata.monotonicFrameId();lastEpoch=metadata.discontinuityEpoch();dispatched++;
            event("PREPARED realFrameId="+lastId+" timestamp="+metadata.realFrameTimestamp()+" deltaMs="+metadata.realFrameDeltaMs()+" epoch="+lastEpoch+" reset="+reset+" slot="+slot.index+" VkImage="+slot.generated.get(0).handle()+" copies=4 outputCopies=0 shadow="+shadow+" sample="+sample+" inputKey="+key+" motionJittered="+snapshot.constants().motionVectorsJittered()+" jitter="+snapshot.constants().jitterOffsetX()+","+snapshot.constants().jitterOffsetY());
            Lease lease=new Lease(slot,input.device(),frame.readySemaphores(),wait,metadata.monotonicFrameId(),metadata.discontinuityEpoch(),reset,shadow,sample);
            return FrameGenerationDispatchResult.success(count,lease,reset?FrameGenerationDispatchResult.HistoryDisposition.RESET:FrameGenerationDispatchResult.HistoryDisposition.UNCHANGED);
        }catch(Throwable error){
            if(slot!=null){try{DlssgBridge.abort(session,slot.pool.nativePool,slot.index);slot.leased=false;}catch(Throwable aborted){error.addSuppressed(aborted);}}
            fail(stage,error);return FrameGenerationDispatchResult.failed(stage+": "+error);
        }
    }
    private void collectRetiredPools(){for(Pool p:pools)if(p.retired&&!p.closed&&Arrays.stream(p.slots).noneMatch(s->s.leased)){DlssgBridge.closePool(session,p.nativePool);p.closed=true;}}
    private record Key(int w,int h,int rw,int rh,int count,int color,int hudless,int motion,boolean flip){}
    private final class Pool{
        final Key key;final long nativePool;final Slot[] slots;boolean retired,closed;
        Pool(VulkanDevice device,Key k){key=k;int count=AsyncFramePresenter.maximumLiveProviderLeases(k.count);nativePool=DlssgBridge.createPool(session,k.w,k.h,k.rw,k.rh,count,k.count);slots=new Slot[count];for(int i=0;i<count;i++)slots[i]=new Slot(this,i,device);}
        Slot acquire(){for(Slot s:slots)if(!s.leased){s.leased=true;
            event("POOL_LEASE slot="+s.index+" inFlight="+Arrays.stream(slots).filter(x->x.leased).count()+" capacity="+slots.length);return s;}
            event("POOL_EXHAUSTION capacity="+slots.length);throw new IllegalStateException("Derived shared-pool capacity exhausted; no CPU stall or reuse");}
    }
    private final class Slot{
        final Pool pool;final int index;final VulkanTexture real;final List<VulkanTexture> generated;boolean leased;
        Slot(Pool p,int index,VulkanDevice device){pool=p;this.index=index;real=wrap(device,p,index,0);var outputs=new ArrayList<VulkanTexture>();for(int k=0;k<p.key.count;k++)outputs.add(wrap(device,p,index,4+k));generated=List.copyOf(outputs);}
    }
    private VulkanTexture wrap(VulkanDevice device,Pool pool,int slot,int role){
        long[] handles=DlssgBridge.image(pool.nativePool,slot,role);
        var description=TextureDescription.create().width(pool.key.w).height(pool.key.h).format(TextureFormat.RGBA8)
            .type(TextureType.Texture2D).usages(TextureUsages.create().sampler().storage().transferSource().transferDestination()).build();
        return VulkanTexture.wrapBorrowedImage(device,description,handles[0],handles[1],handles[2],VK_IMAGE_LAYOUT_UNDEFINED);
    }
    private final class Lease implements FrameGenerationProviderOutput,FrameGenerationSubmissionPlan{
        final Slot slot;final VulkanDevice device;final long[] captureReady,wait;final long id,epoch;
        final boolean reset,sampled;final DlssgOutputStatus status;
        boolean submitted,released;
        Lease(Slot s,VulkanDevice d,long[] ready,long[] wait,long id,long epoch,boolean reset,boolean shadow,boolean sampled){
            slot=s;device=d;captureReady=ready.clone();this.wait=wait.clone();this.id=id;this.epoch=epoch;this.reset=reset;this.sampled=sampled;
            status=new DlssgOutputStatus(id,wait[1],s.generated.size(),reset||shadow,flags->{
                for(int k=0;k<flags.length;k++){
                    String state=flags[k]==0?"ENABLED":flags[k]==1?"DISABLED":"UNKNOWN";
                    event("OUTPUT_READY realFrameId="+id+" previousRealFrameId="+(id-1)+" index="+(k+1)+" count="+flags.length+" rawStatus="+Integer.toUnsignedString(flags[k])+" status="+state+" disable="+flags[k]+" reset="+reset+" epoch="+epoch+" completion="+wait[1]+" completionScope=GROUP_ORDERED_COMMAND_LIST presentable="+(flags[k]==0&&!reset&&!shadow)+" VkImage="+slot.generated.get(k).handle());
                    if("UNKNOWN".equals(state)&&!diagnosticCapture.enabled())fail("OUTPUT_STATUS_NOT_CONFIRMED_INDEX_"+(k+1),new IllegalStateException("STATUS_NOT_WRITTEN_OR_UNCONSUMED raw="+Integer.toUnsignedString(flags[k])+" realFrameId="+id));
                }
            });
        }
        public List<VulkanTexture> generatedOutputs(){return slot.generated;}
        public java.util.concurrent.CompletableFuture<Void> presentationReadiness(){return status.readiness();}
        public boolean isGeneratedOutputPresentable(int index){return status.presentable(index);}
        public void onGeneratedOutputDiscarded(int index,long now){event("DISCARD realFrameId="+id+" generatedIndex="+(index+1)+" count="+slot.generated.size()+" disable="+status.flag(index)+" reset="+reset+" epoch="+epoch+" timestamp="+now+" lifetime=retained_until_all_gpu_fences");}
        public VulkanTexture realOutput(){return slot.real;}
        public FrameGenerationDispatchCompletion completion(){return FrameGenerationDispatchCompletion.completed();}
        public OutputKey outputKey(){return new OutputKey(slot.pool.key.w,slot.pool.key.h,VK_FORMAT_R8G8B8A8_UNORM);}
        public boolean isReleased(){return released;}
        public FrameGenerationSubmissionPlan submissionPlan(){return this;}
        public boolean inputsSubmitted(){return submitted;}
        public TimelineWait submitInputs(){
            requireOwner();if(submitted||released)throw new IllegalStateException("Lease submitted twice");submitted=true;
            try{synchronized(device.requireFgQueue().submitLock()){DlssgBridge.submit(session,slot.pool.nativePool,slot.index,captureReady,status);}
                event("SUBMITTED realFrameId="+id+" slot="+slot.index+" done="+wait[1]+" presentation="+(shadow?"REAL_ONLY":"GENERATED_THEN_REAL"));
                return new TimelineWait(wait[0],wait[1]);
            }catch(Throwable e){fail("GPU_SUBMISSION",e);throw e;}
        }
        public void release(){requireOwner();if(released)return;
            try{DlssgBridge.release(session,slot.pool.nativePool,slot.index);released=true;slot.leased=false;event("RETIRED realFrameId="+id+" slot="+slot.index);
                if(sampled&&diagnosticCapture.completedSample()){healthy=false;event("DIAGNOSTIC_CAPTURE_COMPLETE realFrameId="+id+" presentationValidityUnchanged=true");}
                collectRetiredPools();}
            catch(Throwable e){fail("LEASE_RETIREMENT",e);throw e;}
        }
        public void abort(){requireOwner();if(released)return;
            if(submitted){fail("ABORT_AFTER_GPU_SUBMISSION",new IllegalStateException("Retain native pool until safe terminal drain"));return;}
            DlssgBridge.abort(session,slot.pool.nativePool,slot.index);released=true;slot.leased=false;invalidateHistory();
        }
        public void onPresented(long displayIndex,int outputIndex,boolean generated,long requestNs,long queueDelayNs,long deadlineNs){
            int index=generated?outputIndex+1:0;
            event("PRESENT realFrameId="+id+" previousRealFrameId="+(id-1)+" generatedIndex="+index+
                " multiFrameCount="+slot.generated.size()+" epoch="+epoch+" reset="+reset+" displayIndex="+displayIndex+" kind="+(generated?"GENERATED":"REAL")+
                " VkImage="+(generated?slot.generated.get(outputIndex).handle():slot.real.handle())+" timestamp="+requestNs+
                " queueDelayNs="+queueDelayNs+" deadlineNs="+deadlineNs+" lateNs="+(deadlineNs==0?-1:Math.max(0,requestNs-deadlineNs))+
                " latencyMethod=CPU_batch_publication_to_present_worker_entry;request_not_scanout");
        }
        public void onOutputSubmitted(long commandBuffer,long fence){
            event("VULKAN_CONSUMER_SUBMIT realFrameId="+id+" slot="+slot.index+" commandBuffer="+commandBuffer+
                " fence="+fence+" timelineWait="+wait[1]+" G1="+slot.generated.get(0).handle());
        }
    }
    void close(){requireOwner();if(session!=0){try{DlssgBridge.close(session);session=0;}catch(Throwable e){fail("TEARDOWN",e);}}
        if(events!=null){events.flush();events.close();events=null;}
        if(jsonEvents!=null){jsonEvents.flush();jsonEvents.close();jsonEvents=null;}
        if(session==0){pools.clear();current=null;lastId=lastEpoch=-1;invalidHistory=true;owner=null;}}
}
