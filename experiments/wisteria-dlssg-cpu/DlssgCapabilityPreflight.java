package org.ireallywanttosleep.wisteria.dlssg;

/** Isolated fresh capability query only. No feature, no dispatch, no Minecraft. */
public final class DlssgCapabilityPreflight {
    public static void main(String[] args) {
        if(!"25.0.4".equals(System.getProperty("java.version")))throw new IllegalStateException("Java25.0.4 required");
        System.load(java.nio.file.Path.of(args[0]).toAbsolutePath().toString());
        if(DlssgBridge.abi()!=2)throw new IllegalStateException("Own JNI ABI mismatch");
        int[] values=DlssgBridge.probeCapabilities(args[1],args[2],args[3],Integer.parseInt(args[4]));
        System.out.println("PREFLIGHT_JAVA_VERSION="+System.getProperty("java.version"));
        System.out.println("PREFLIGHT_JAVA_HOME="+System.getProperty("java.home"));
        System.out.println("FG_AVAILABLE="+values[0]);System.out.println("RAW_REPORTED_MULTI_FRAME_COUNT_MAX="+values[1]);
        System.out.println("AVAILABLE_GETTER_RESULT="+values[2]);System.out.println("MAX_GETTER_RESULT="+values[3]);
        if(values[2]!=1||values[3]!=1||values[0]!=1||values[1]<Integer.parseInt(args[4]))throw new IllegalStateException("Capability gate failed; do not launch Minecraft");
        System.out.println("PREFLIGHT_CAPABILITY_GATE=PASS; CREATEFEATURE_CALLS=0; EVALUATE_CALLS=0; GENERATION_NOT_PROVEN");
    }
}
