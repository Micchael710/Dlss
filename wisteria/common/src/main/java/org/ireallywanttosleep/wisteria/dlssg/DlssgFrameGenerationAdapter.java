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
    private int dispatched,samples;
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
    private void event(String message){if(events!=null){events.println(System.nanoTime()+" "+message);if(message.startsWith("RETIRED")&&dispatched%120==0)events.flush();}}
    private void initialize(VulkanDevice device){
        if(session!=0)return;
        try{Path out=Path.of(System.getProperty("wisteria.dlssg.runDir")).toAbsolutePath();
            Files.createDirectories(out);events=new PrintWriter(Files.newBufferedWriter(out.resolve("frame-sequence.log")));
            session=DlssgBridge.create(device.getVkInstance().address(),device.getPhysicalDevice().address(),device.getVkDevice().address(),
                device.requireFgQueue().getQueue().address(),device.requireFgQueue().getQueueFamilyIndex(),
                VK.getFunctionProvider().getFunctionAddress("vkGetInstanceProcAddr"),out.toString(),
                System.getProperty("wisteria.dlssg.externalDll"),System.getProperty("wisteria.dlssg.runtimeDir"));
            event("SESSION persistent=true shadow="+shadow+" outputPoolSlots="+AsyncFramePresenter.maximumLiveProviderLeases(0));
        }catch(Exception e){throw new IllegalStateException("DLSS-G session initialization failed",e);}
    }
    FrameGenerationDispatchResult dispatch(FrameGenerationDispatchInput input){
        requireOwner();if(!healthy)return FrameGenerationDispatchResult.failed("DLSS-G experiment latched unavailable; no retry");
        Slot slot=null;
        String stage="INPUT_MAPPING";
        try{
            var snapshot=(DlssgFrameGenerationBackend.Snapshot)input.providerInputSnapshot();var metadata=Objects.requireNonNull(snapshot.metadata());
            var frame=input.frameResources();
            // The controlled baseline uses OpenGL FSR1 and owned capture inputs. A
            // Vulkan upscaler can read its borrowed inputs concurrently on the main
            // queue; changing their layouts for transfer requires a separate audited join.
            if(frame.hasBorrowedAlgorithmInputs())throw new IllegalStateException("Borrowed Vulkan-upscaler inputs need an audited queue join; use the unchanged owned-capture baseline");
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
            Key key=new Key(w,h,rw,rh,sources[0].getTextureFormat().vk(),sources[1].getTextureFormat().vk(),sources[3].getTextureFormat().vk(),flip);
            if(current==null||!current.key.equals(key)){
                stage="POOL_CREATE_FEATURE";if(current!=null)current.retired=true;
                current=new Pool(input.device(),key);pools.add(current);invalidHistory=true;
                event("POOL_CREATE key="+key+" persistentHandles=true");
            }
            collectRetiredPools();slot=current.acquire();stage="PREPARE_EVALUATE";
            boolean reset=invalidHistory||snapshot.historyResetRequested()||metadata.discontinuityEpoch()!=lastEpoch||metadata.monotonicFrameId()!=lastId+1;
            long[] nativeSources=new long[20];for(int j=0;j<4;j++){
                var src=sources[j];int expectedW=j<2?w:rw,expectedH=j<2?h:rh;
                if(src.getWidth()!=expectedW||src.getHeight()!=expectedH)throw new IllegalStateException("Resource/metadata extent mismatch");
                int off=j*5;nativeSources[off]=src.handle();nativeSources[off+1]=src.getWidth();nativeSources[off+2]=src.getHeight();nativeSources[off+3]=src.getTextureFormat().vk();nativeSources[off+4]=src.getCurrentLayout();
            }
            boolean sample=dispatched>=30&&samples<3&&slot.index<3;if(sample)samples++;
            long[] wait=DlssgBridge.prepare(session,current.nativePool,slot.index,input.commandBuffer(),nativeSources,constants,
                metadata.monotonicFrameId(),metadata.realFrameDeltaMs(),reset,flip,sample);
            slot.real.setCurrentLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);slot.generated.setCurrentLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            invalidHistory=false;lastId=metadata.monotonicFrameId();lastEpoch=metadata.discontinuityEpoch();dispatched++;
            event("PREPARED realFrameId="+lastId+" timestamp="+metadata.realFrameTimestamp()+" deltaMs="+metadata.realFrameDeltaMs()+" epoch="+lastEpoch+" reset="+reset+" slot="+slot.index+" VkImage="+slot.generated.handle()+" copies=4 outputCopies=0 shadow="+shadow+" sample="+sample+" inputKey="+key+" motionJittered="+snapshot.constants().motionVectorsJittered()+" jitter="+snapshot.constants().jitterOffsetX()+","+snapshot.constants().jitterOffsetY());
            Lease lease=new Lease(slot,input.device(),frame.readySemaphores(),wait,metadata.monotonicFrameId(),!shadow&&!reset);
            return FrameGenerationDispatchResult.success(shadow||reset?0:1,lease,reset?FrameGenerationDispatchResult.HistoryDisposition.RESET:FrameGenerationDispatchResult.HistoryDisposition.UNCHANGED);
        }catch(Throwable error){
            if(slot!=null){try{DlssgBridge.abort(session,slot.pool.nativePool,slot.index);slot.leased=false;}catch(Throwable aborted){error.addSuppressed(aborted);}}
            fail(stage,error);return FrameGenerationDispatchResult.failed(stage+": "+error);
        }
    }
    private void collectRetiredPools(){for(Pool p:pools)if(p.retired&&!p.closed&&Arrays.stream(p.slots).noneMatch(s->s.leased)){DlssgBridge.closePool(session,p.nativePool);p.closed=true;}}
    private record Key(int w,int h,int rw,int rh,int color,int hudless,int motion,boolean flip){}
    private final class Pool{
        final Key key;final long nativePool;final Slot[] slots;boolean retired,closed;
        Pool(VulkanDevice device,Key k){key=k;int count=AsyncFramePresenter.maximumLiveProviderLeases(0);nativePool=DlssgBridge.createPool(session,k.w,k.h,k.rw,k.rh,count);slots=new Slot[count];for(int i=0;i<count;i++)slots[i]=new Slot(this,i,device);}
        Slot acquire(){for(Slot s:slots)if(!s.leased){s.leased=true;return s;}throw new IllegalStateException("Derived shared-pool capacity exhausted; no CPU stall or reuse");}
    }
    private final class Slot{
        final Pool pool;final int index;final VulkanTexture real,generated;boolean leased;
        Slot(Pool p,int index,VulkanDevice device){pool=p;this.index=index;real=wrap(device,p,index,0);generated=wrap(device,p,index,4);}
    }
    private VulkanTexture wrap(VulkanDevice device,Pool pool,int slot,int role){
        long[] handles=DlssgBridge.image(pool.nativePool,slot,role);
        var description=TextureDescription.create().width(pool.key.w).height(pool.key.h).format(TextureFormat.RGBA8)
            .type(TextureType.Texture2D).usages(TextureUsages.create().sampler().storage().transferSource().transferDestination()).build();
        return VulkanTexture.wrapBorrowedImage(device,description,handles[0],handles[1],handles[2],VK_IMAGE_LAYOUT_UNDEFINED);
    }
    private final class Lease implements FrameGenerationProviderOutput,FrameGenerationSubmissionPlan{
        final Slot slot;final VulkanDevice device;final long[] captureReady,wait;final long id;final boolean generated;
        boolean submitted,released;
        Lease(Slot s,VulkanDevice d,long[] ready,long[] wait,long id,boolean generated){slot=s;device=d;captureReady=ready.clone();this.wait=wait.clone();this.id=id;this.generated=generated;}
        public List<VulkanTexture> generatedOutputs(){return generated?List.of(slot.generated):List.of();}
        public VulkanTexture realOutput(){return slot.real;}
        public FrameGenerationDispatchCompletion completion(){return FrameGenerationDispatchCompletion.completed();}
        public OutputKey outputKey(){return new OutputKey(slot.pool.key.w,slot.pool.key.h,VK_FORMAT_R8G8B8A8_UNORM);}
        public boolean isReleased(){return released;}
        public FrameGenerationSubmissionPlan submissionPlan(){return this;}
        public boolean inputsSubmitted(){return submitted;}
        public TimelineWait submitInputs(){
            requireOwner();if(submitted||released)throw new IllegalStateException("Lease submitted twice");submitted=true;
            try{synchronized(device.requireFgQueue().submitLock()){DlssgBridge.submit(session,slot.pool.nativePool,slot.index,captureReady);}
                event("SUBMITTED realFrameId="+id+" slot="+slot.index+" done="+wait[1]+" presentation="+(shadow?"REAL_ONLY":"GENERATED_THEN_REAL"));
                return new TimelineWait(wait[0],wait[1]);
            }catch(Throwable e){fail("GPU_SUBMISSION",e);throw e;}
        }
        public void release(){requireOwner();if(released)return;
            try{DlssgBridge.release(session,slot.pool.nativePool,slot.index);released=true;slot.leased=false;event("RETIRED realFrameId="+id+" slot="+slot.index);collectRetiredPools();}
            catch(Throwable e){fail("LEASE_RETIREMENT",e);throw e;}
        }
        public void abort(){requireOwner();if(released)return;
            if(submitted){fail("ABORT_AFTER_GPU_SUBMISSION",new IllegalStateException("Retain native pool until safe terminal drain"));return;}
            DlssgBridge.abort(session,slot.pool.nativePool,slot.index);released=true;slot.leased=false;invalidateHistory();
        }
        public void onPresented(long displayIndex,boolean generated,long requestNs,long queueDelayNs){
            event("PRESENT realFrameId="+id+" previousRealFrameId="+(id-1)+" generatedIndex="+(generated?1:0)+
                " multiFrameCount=1 displayIndex="+displayIndex+" kind="+(generated?"GENERATED":"REAL")+
                " VkImage="+(generated?slot.generated.handle():slot.real.handle())+" timestamp="+requestNs+
                " queueDelayNs="+queueDelayNs+" latencyMethod=CPU_batch_publication_to_present_worker_entry");
        }
        public void onOutputSubmitted(long commandBuffer,long fence){
            event("VULKAN_CONSUMER_SUBMIT realFrameId="+id+" slot="+slot.index+" commandBuffer="+commandBuffer+
                " fence="+fence+" timelineWait="+wait[1]+" G1="+slot.generated.handle());
        }
    }
    void close(){requireOwner();if(session!=0){try{DlssgBridge.close(session);session=0;}catch(Throwable e){fail("TEARDOWN",e);}}
        if(events!=null){events.flush();events.close();events=null;}
        if(session==0){pools.clear();current=null;lastId=lastEpoch=-1;invalidHistory=true;owner=null;}}
}
