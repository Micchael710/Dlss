package io.homo.superresolution.core.graphics.vulkan;

/** Host submission identity, not a claim of GPU completion. */
public final class GlReleaseContract {
    public record Submission(int slot,long generation,int logicalFrame,long realFrameId,
                             long command,long fence,long depthSemaphore,long motionSemaphore) {}
    private long expectedGeneration, previousWaitFrame = -1;
    private int expectedSlot, expectedFrame;
    private boolean pending, waited;
    public void begin(int slot,long generation,int frame) {
        if (pending || generation <= 0) throw new IllegalStateException("Source reuse before GL release");
        expectedSlot=slot;expectedGeneration=generation;expectedFrame=frame;pending=true;waited=false;
    }
    public void validate(Submission s,long handle,int[] textures,int[] layouts) {
        if(!pending||waited||s==null||s.slot()!=expectedSlot||s.generation()!=expectedGeneration
                ||s.logicalFrame()!=expectedFrame||s.command()==0||s.fence()==0||handle==0
                ||(handle!=s.depthSemaphore()&&handle!=s.motionSemaphore()))
            throw new IllegalStateException("Wrong/stale/unsubmitted GL release identity");
        if(textures==null||layouts==null||textures.length!=layouts.length||textures.length!=1
                ||textures[0]==0||layouts[0]!=0x9591)
            throw new IllegalStateException("Invalid release texture/layout list");
    }
    public void consumed(Submission s) {
        if(!pending||waited||s.slot()!=expectedSlot||s.generation()!=expectedGeneration||s.logicalFrame()!=expectedFrame)throw new IllegalStateException("Duplicate/stale release wait");
        waited=true;pending=false;previousWaitFrame=s.logicalFrame();
    }
    public long previousWaitFrame(){return previousWaitFrame;}
    public void retiredAfterTerminalDrain(){pending=false;waited=true;}
}
