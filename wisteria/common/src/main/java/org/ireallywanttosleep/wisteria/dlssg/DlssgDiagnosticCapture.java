package org.ireallywanttosleep.wisteria.dlssg;

/** Bounded image observation only. Never grants presentation validity. */
final class DlssgDiagnosticCapture {
    static final int WARMUP_GROUPS=30, MAX_DISPATCHES=600, MAX_SAMPLES=4;
    private final boolean enabled;
    private int requested, completed;
    DlssgDiagnosticCapture(boolean enabled){this.enabled=enabled;}
    boolean enabled(){return enabled;}
    boolean select(int dispatched,boolean reset,double cameraMotion){
        if(!enabled||reset||dispatched<WARMUP_GROUPS||requested>=MAX_SAMPLES)return false;
        if(requested==0&&cameraMotion<=0.01)return false;
        requested++;return true;
    }
    boolean limitReached(int dispatched){return enabled&&(completed>=MAX_SAMPLES||dispatched>=MAX_DISPATCHES);}
    boolean completedSample(){return enabled&&++completed>=MAX_SAMPLES;}
}
