package io.homo.superresolution.core.graphics.vulkan;
import com.google.gson.JsonObject;
import java.nio.file.*;
import java.io.PrintWriter;

/** Exact GL-call correlation; bounded errors and flushed native teardown markers. */
public final class InteropReleaseDiagnostics {
    private static final String DIR=System.getProperty("sr.dlss.runDir");
    private static PrintWriter errors, stages;
    private static long errorCount, waitCount;
    private static final ThreadLocal<String> active=new ThreadLocal<>();
    public static boolean enabled(){return DIR!=null && Boolean.getBoolean("sr.dlss.glReleaseDiagnostics");}
    public static void active(String text){active.set(text);}
    public static void clear(){active.remove();}
    public static String current(){return active.get();}
    public static synchronized void waitResult(GlReleaseContract.Submission s,String resource,long gl,long vk,
            long semaphoreGeneration,int texture,int layout,int code,int preError,long previousWait,
            boolean validSemaphore,boolean validTexture,long context) {
        if(!enabled())return;waitCount++;
        if(code!=0)errorCount++;
        if(code==0&&waitCount>10)return;
        if(code!=0&&errorCount>10)return;
        try{
            if(errors==null)errors=new PrintWriter(Files.newBufferedWriter(Path.of(DIR,"gl-release-waits.jsonl")));
            JsonObject j=new JsonObject();j.addProperty("logicalFrame",s.logicalFrame());j.addProperty("realFrameId",s.realFrameId());
            j.addProperty("captureGeneration",s.generation());j.addProperty("slot",s.slot());j.addProperty("resource",resource);
            j.addProperty("glSemaphoreHandle",gl);j.addProperty("vulkanSemaphoreHandle",vk);j.addProperty("textureHandle",texture);
            j.addProperty("textureCount",1);j.addProperty("layout",layout);j.addProperty("glError",code);j.addProperty("glErrorName",errorName(code));
            j.addProperty("preexistingError",preError);j.addProperty("thread",Thread.currentThread().getName());j.addProperty("context",context);
            j.addProperty("semaphoreGeneration",semaphoreGeneration);j.addProperty("signalFrame",s.logicalFrame());j.addProperty("previousWaitFrame",previousWait);
            j.addProperty("signalSubmitted",true);j.addProperty("signalCommand",s.command());j.addProperty("signalFence",s.fence());
            j.addProperty("validSemaphore",validSemaphore);j.addProperty("validTexture",validTexture);errors.println(j);errors.flush();
        }catch(java.io.IOException e){throw new IllegalStateException(e);}
    }
    public static String errorName(int e){return switch(e){case 0->"GL_NO_ERROR";case 0x500->"GL_INVALID_ENUM";case 0x501->"GL_INVALID_VALUE";case 0x502->"GL_INVALID_OPERATION";case 0x505->"GL_OUT_OF_MEMORY";default->"GL_ERROR_"+e;};}
    public static synchronized void stage(String stage){
        if(!enabled())return;
        try{if(stages==null)stages=new PrintWriter(Files.newBufferedWriter(Path.of(DIR,"shutdown-stages.jsonl")));
            JsonObject j=new JsonObject();j.addProperty("stage",stage);j.addProperty("timestamp_ns",System.nanoTime());j.addProperty("thread",Thread.currentThread().getName());stages.println(j);stages.flush();
            JsonObject summary=new JsonObject();summary.addProperty("wait_count",waitCount);summary.addProperty("gl_sync_error_count",errorCount);
            Files.writeString(Path.of(DIR,"gl-release-summary.json"),summary.toString()+"\n");
        }catch(java.io.IOException e){throw new IllegalStateException(e);}
    }
    public static void run(String name,Runnable action){stage(name+"_BEGIN");action.run();stage(name+"_END");}
}
