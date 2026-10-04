package org.ireallywanttosleep.wisteria.dlssg;
public final class DlssgDiagnosticCaptureTest {
    private static void check(boolean b){if(!b)throw new AssertionError();}
    public static void main(String[] args){
        var off=new DlssgDiagnosticCapture(false);check(!off.select(31,false,1));check(!off.limitReached(601));
        var capture=new DlssgDiagnosticCapture(true);
        check(!capture.select(29,false,1));check(!capture.select(30,true,1));check(!capture.select(30,false,0));
        for(int i=0;i<4;i++)check(capture.select(30+i,false,i==0?1:0));
        check(!capture.select(34,false,1));check(!capture.limitReached(34));
        for(int i=0;i<4;i++)check(capture.completedSample()==(i==3));
        check(capture.limitReached(34));check(new DlssgDiagnosticCapture(true).limitReached(600));
        var status=new DlssgOutputStatus(34,68,2,false,f->{});
        status.completeFromNative(34,new int[]{0,-1},68);check(!status.presentable(1));
        System.out.println("PASS CPU: diagnostic warmup/motion/reset, four-request/readback retirement limit, dispatch ceiling, disabled default, UNKNOWN remains unpresentable");
    }
}
