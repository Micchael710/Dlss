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
        var missing=new DlssgOutputStatus(1,2,1,false,f->{});missing.completeFromNative(1,new int[]{-1},2);
        check(missing.readiness().isCompletedExceptionally());
        System.out.println("PASS: disabled-output gating, immutable flags, count1..4, group reset and wrong-interval rejection");
    }
}
