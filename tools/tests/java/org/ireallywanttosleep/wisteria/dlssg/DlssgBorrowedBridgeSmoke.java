package org.ireallywanttosleep.wisteria.dlssg;

/** Actual JNI load/ABI gate, with no NGX init, feature, GPU, or Minecraft execution. */
public final class DlssgBorrowedBridgeSmoke {
    public static void main(String[] args) throws Exception {
        System.load(java.nio.file.Path.of(args[0]).toAbsolutePath().toString());
        var method=Class.forName("org.ireallywanttosleep.wisteria.dlssg.DlssgBridge").getDeclaredMethod("abi");
        method.setAccessible(true);
        int abi=(Integer)method.invoke(null);
        if(abi!=4)throw new AssertionError("Stale JNI payload contract: "+abi);
        System.out.println("PASS ACTUAL JNI LOAD ABI="+abi+"; NGX/feature/evaluate NOT_RUN");
    }
}
