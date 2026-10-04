package org.ireallywanttosleep.wisteria.dlssg;

import java.nio.file.*;
import java.security.MessageDigest;
import java.util.HexFormat;

/** Own JNI is packaged; externally licensed runtimes are supplied by the local test launcher. */
final class DlssgBridge {
    private static boolean loaded;
    static synchronized void load() {
        if (loaded) return;
        try {
            String resource = "/natives/windows-x64/wisteria_dlssg_bridge.dll";
            byte[] bytes;
            try (var stream = DlssgBridge.class.getResourceAsStream(resource)) {
                if (stream == null) throw new IllegalStateException("Experimental JNI bridge not packaged");
                bytes = stream.readAllBytes();
            }
            String hash = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
            Path root = Path.of(System.getProperty("wisteria.dlssg.runDir")).toAbsolutePath();
            Files.createDirectories(root.resolve("native"));
            Path dll = root.resolve("native/wisteria_dlssg_bridge-" + hash + ".dll");
            Files.write(dll, bytes);
            System.load(dll.toString());
            if (abi() != 4) throw new IllegalStateException("DLSS-G JNI ABI mismatch");
            Files.writeString(root.resolve("jni-manifest.json"), "{\"abi\":4,\"sha256\":\"" + hash + "\"}\n");
            loaded = true;
        } catch (Exception e) { throw new IllegalStateException("DLSS-G local bridge load failed", e); }
    }
    static native int abi();
    static native void configureDiagnostics(long session,String out,boolean fault,boolean checkpoints,boolean sync2);
    static native int reportedMax(long session);
    /** Isolated preflight: public NGX init/query only, no feature or Evaluate. */
    static native int[] probeCapabilities(String out,String dll,String runtime,int requestedMax);
    static native long create(long instance,long physical,long device,long queue,int family,long gipa,
                              String out,String dll,String runtime,int requestedMax);
    static native long createPool(long session,int w,int h,int rw,int rh,int slots,int count);
    static native long[] image(long pool,int slot,int role);
    static native long[] prepare(long session,long pool,int slot,long command,long[] sources,float[] constants,
                                 long frameId,double delta,boolean reset,boolean flip,boolean sample);
    static native void submit(long session,long pool,int slot,long[] readySemaphores,DlssgOutputStatus status);
    static native void release(long session,long pool,int slot);
    static native void abort(long session,long pool,int slot);
    static native void closePool(long session,long pool);
    static native void close(long session);
}
