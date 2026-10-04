package io.homo.superresolution.core.ngx;
/** Actual JNI loader/export smoke only; no GPU feature or capability simulation. */
public final class NgxBindingLoadSmoke {
    public static void main(String[] args){
        if(args.length>1&&args[1].equals("missing")){
            try{System.load(args[0]);throw new AssertionError("Expected the old binding path to be absent");}
            catch(UnsatisfiedLinkError e){System.out.println("OBSERVED_OLD_NATIVE_LOAD_ERROR="+e);return;}
        }
        System.load(args[0]);
        int a=NgxNative.nDestroyParameters(0),b=NgxNative.nReleaseFeature(0);
        if(NgxConstants.succeeded(a)||NgxConstants.succeeded(b))throw new AssertionError("Null handles must fail");
        System.out.println("PASS ACTUAL JNI LOAD + null-handle safety; init/create/evaluate=NOT_RUN; result="+Integer.toUnsignedString(a));
    }
}
