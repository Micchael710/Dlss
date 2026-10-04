package org.ireallywanttosleep.wisteria.dlssg;
public final class DlssgOutputStatusTest {
    private static void check(boolean value){if(!value)throw new AssertionError();}
    public static void main(String[] args){
        for(int count=1;count<=4;count++){
            var status=new DlssgOutputStatus(4037,8070,count,false,flags->{});
            check(!status.readiness().isDone());
            int[] flags=new int[count];flags[0]=1;
            status.completeFromNative(4037,flags,8070);flags[0]=0;
            check(!status.presentable(0)); // frame4037 regression; caller mutation cannot enable it
            for(int i=1;i<count;i++)check(status.presentable(i));
            status.completeFromNative(4037,new int[count],8070);check(!status.presentable(0));
            var reset=new DlssgOutputStatus(1,2,count,true,f->{});
            reset.completeFromNative(1,new int[count],2);for(int i=0;i<count;i++)check(!reset.presentable(i));
        }
        var good=new DlssgOutputStatus(2,4,1,false,f->{});good.completeFromNative(2,new int[]{0},4);check(good.presentable(0));
        var wrong=new DlssgOutputStatus(4037,8070,2,false,f->{});wrong.completeFromNative(4036,new int[]{0,0},8068);
        check(wrong.readiness().isCompletedExceptionally());
        var missing=new DlssgOutputStatus(1,2,2,false,f->{});missing.completeFromNative(1,new int[]{0,-1},2);
        check(missing.readiness().isDone()&&!missing.readiness().isCompletedExceptionally());
        check(missing.presentable(0)&&!missing.presentable(1));check(missing.state(1).equals("UNKNOWN"));
        var later=new DlssgOutputStatus(2,4,2,false,f->{});later.completeFromNative(2,new int[]{-1,0},4);
        check(!later.presentable(0)&&later.presentable(1)); // independent status, not one group boolean
        for(int index:new int[]{-1,2}){boolean rejected=false;try{missing.presentable(index);}catch(IndexOutOfBoundsException expected){rejected=true;}check(rejected);}
        var wrongFence=new DlssgOutputStatus(1,2,2,false,f->{});wrongFence.completeFromNative(1,new int[]{0,0},4);
        check(wrongFence.readiness().isCompletedExceptionally());
        var wrongCount=new DlssgOutputStatus(1,2,2,false,f->{});wrongCount.completeFromNative(1,new int[]{0},2);
        check(wrongCount.readiness().isCompletedExceptionally());
        var invalid=new DlssgOutputStatus(1,2,2,false,f->{});invalid.completeFromNative(1,new int[]{2,1},2);
        check(!invalid.presentable(0)&&invalid.state(0).equals("UNKNOWN"));check(invalid.state(1).equals("DISABLED"));
        System.out.println("PASS: independent count1..4 status, disable1, immutable flags, sentinel UNKNOWN/discard, index bounds, wrong interval/fence/count, reset and synchronous 1-output compatibility");
    }
}
