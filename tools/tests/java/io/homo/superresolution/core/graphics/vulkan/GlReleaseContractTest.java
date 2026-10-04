package io.homo.superresolution.core.graphics.vulkan;
public final class GlReleaseContractTest {
    private static int rejected;
    private static void rejects(Runnable r){try{r.run();}catch(IllegalStateException expected){rejected++;return;}throw new AssertionError("Unsafe GL release accepted");}
    public static void main(String[] args){
        var c=new GlReleaseContract();c.begin(2,10,42);
        var s=new GlReleaseContract.Submission(2,10,42,7,100,101,11,12);
        c.validate(s,11,new int[]{20},new int[]{0x9591});
        rejects(()->c.validate(new GlReleaseContract.Submission(3,10,42,7,100,101,11,12),11,new int[]{20},new int[]{0x9591}));
        rejects(()->c.validate(new GlReleaseContract.Submission(2,9,42,7,100,101,11,12),11,new int[]{20},new int[]{0x9591}));
        rejects(()->c.validate(new GlReleaseContract.Submission(2,10,41,7,100,101,11,12),11,new int[]{20},new int[]{0x9591}));
        rejects(()->c.validate(s,0,new int[]{20},new int[]{0x9591}));
        rejects(()->c.validate(s,13,new int[]{20},new int[]{0x9591}));
        rejects(()->c.validate(s,11,new int[]{20,21},new int[]{0x9591}));
        rejects(()->c.validate(s,11,new int[]{0},new int[]{0x9591}));
        rejects(()->c.validate(s,11,new int[]{20},new int[]{0x958d}));
        rejects(()->c.validate(null,11,new int[]{20},new int[]{0x9591}));
        rejects(()->c.begin(2,11,43));
        c.consumed(s);rejects(()->c.validate(s,11,new int[]{20},new int[]{0x9591}));
        rejects(()->c.consumed(s));c.begin(2,11,43);
        System.out.println("PASS GL release CPU identities: "+rejected+" unsafe cases rejected; no GPU completion simulated");
    }
}
