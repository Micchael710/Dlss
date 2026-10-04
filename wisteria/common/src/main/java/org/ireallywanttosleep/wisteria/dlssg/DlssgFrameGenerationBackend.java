package org.ireallywanttosleep.wisteria.dlssg;

import io.homo.superresolution.api.registry.framegeneration.*;
import io.homo.superresolution.common.framegeneration.FrameGenerationMode;
import io.homo.superresolution.common.framegeneration.constants.FrameGenerationConstants;
import io.homo.superresolution.common.presentation.capture.FrameResources;
import org.ireallywanttosleep.wisteria.Wisteria;

/** Local, explicitly enabled experiment; never an automatic substitute for AMD. */
public final class DlssgFrameGenerationBackend implements FrameGenerationProvider {
    private final DlssgFrameGenerationAdapter adapter=new DlssgFrameGenerationAdapter();
    private volatile boolean loaded;
    public FrameGenerationExecutionModel executionModel(){return FrameGenerationExecutionModel.APPLICATION_MANAGED_ASYNC;}
    public void initialize(){
        if(!Boolean.getBoolean("wisteria.dlssg.enabled"))return;
        try{DlssgBridge.load();loaded=true;}catch(Throwable e){adapter.fail("JNI_LOAD",e);}
    }
    public boolean isAvailable(){return loaded&&adapter.healthy();}
    public boolean isDependenciesSatisfied(){return isAvailable();}
    public int supportedGeneratedFrameCount(){return isAvailable()?1:0;}
    public int presentationManagedGeneratedFrameCount(FrameGenerationMode mode){return Math.min(mode.generatedFrameCount(),supportedGeneratedFrameCount());}
    public ProviderInputSnapshot captureInputSnapshot(String id,FrameResources frame,FrameGenerationConstants constants,FrameGenerationMode mode){
        var state=io.homo.superresolution.common.workmode.SRWorkModeManager.getCurrentState();
        if(frame.metadata()==null||!frame.hasFinalColor()||!frame.hasHudlessColor()||!frame.hasDepth()||!frame.hasMotionVector()
                ||state.shaderPackLoading()){
            adapter.invalidateHistory();return null; // startup/menu/loading captures are not NGX jobs
        }
        return new Snapshot(id,frame.logicalFrameIndex(),mode,constants,constants.reset()!=0,frame.metadata(),
                frame.hasBorrowedAlgorithmInputs()&&!io.homo.superresolution.common.config.SuperResolutionConfig.isFlipVkGlInteropResourcesY());
    }
    public FrameGenerationDispatchResult dispatchAsync(FrameGenerationDispatchInput input){return adapter.dispatch(input);}
    public void disable(){adapter.invalidateHistory();}
    public void shutdownOnFrameGenerationThread(){adapter.close();}
    public void shutdown(){}
    record Snapshot(String providerId,int logicalFrameIndex,FrameGenerationMode mode,FrameGenerationConstants constants,
                    boolean historyResetRequested,RealFrameMetadata metadata,boolean flipBorrowedInputs) implements ProviderInputSnapshot {}
}
