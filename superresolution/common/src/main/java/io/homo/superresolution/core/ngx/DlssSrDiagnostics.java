package io.homo.superresolution.core.ngx;

import com.google.gson.JsonObject;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;

/** Opt-in observation of actual calls; never supplies capability or validity. */
public final class DlssSrDiagnostics {
    private static final String DIRECTORY=System.getProperty("sr.dlss.runDir");
    private static PrintWriter writer;
    public static boolean enabled(){return DIRECTORY!=null;}
    public static synchronized void event(String type,Object... pairs){
        if(!enabled())return;
        try{
            if(writer==null){Files.createDirectories(Path.of(DIRECTORY));writer=new PrintWriter(Files.newBufferedWriter(Path.of(DIRECTORY,"dlss-sr-events.jsonl")));
                event("RUNTIME","java",System.getProperty("java.version"),"javaHome",System.getProperty("java.home"),"pid",ProcessHandle.current().pid());}
            JsonObject record=new JsonObject();record.addProperty("type",type);record.addProperty("timestamp_ns",System.nanoTime());
            for(int i=0;i<pairs.length;i+=2)record.addProperty(pairs[i].toString(),String.valueOf(pairs[i+1]));
            writer.println(record);writer.flush();
        }catch(java.io.IOException e){throw new IllegalStateException("Cannot preserve DLSS SR diagnostics",e);}
    }
    public static void save(String name,byte[] data){
        if(!enabled())return;
        try{Files.write(Path.of(DIRECTORY,name),data);}catch(java.io.IOException e){throw new IllegalStateException(e);}
    }
}
