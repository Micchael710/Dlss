package org.ireallywanttosleep.wisteria.dlssg;

import java.util.concurrent.CompletableFuture;
import java.util.function.Consumer;

/** Immutable completion snapshot, published once by the native fence callback. */
final class DlssgOutputStatus {
    private final long realId, completionValue;
    private final int count;
    private final boolean suppress;
    private final Consumer<int[]> observer;
    private final CompletableFuture<Void> ready=new CompletableFuture<>();
    private volatile int[] flags;
    DlssgOutputStatus(long realId,long completionValue,int count,boolean suppress,Consumer<int[]> observer){
        this.realId=realId;this.completionValue=completionValue;this.count=count;
        this.suppress=suppress;this.observer=observer;
    }
    public synchronized void completeFromNative(long id,int[] values,long done){
        if(ready.isDone())return;
        if(id!=realId||done!=completionValue||values.length!=count){
            ready.completeExceptionally(new IllegalStateException("Completion belongs to another interval/output group"));return;
        }
        flags=values.clone();
        try{observer.accept(flags.clone());ready.complete(null);}
        catch(Throwable e){ready.completeExceptionally(e);}
    }
    CompletableFuture<Void> readiness(){return ready;}
    int flag(int index){if(!ready.isDone())throw new IllegalStateException("Output validity not resolved");ready.getNow(null);return flags[index];}
    /** A written API disable flag is 0 or 1. Sentinel/other values remain UNKNOWN. */
    String state(int index){int value=flag(index);return value==0?"ENABLED":value==1?"DISABLED":"UNKNOWN";}
    boolean presentable(int index){return !suppress&&flag(index)==0;}
}
