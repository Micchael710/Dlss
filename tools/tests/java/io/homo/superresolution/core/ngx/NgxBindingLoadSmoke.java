package io.homo.superresolution.core.ngx;
/** Actual JNI loader/export smoke only; no GPU feature or capability simulation. */
public final class NgxBindingLoadSmoke {
    public static void main(String[] args){
        if(args.length>1&&args[1].equals("missing")){
            try{System.load(args[0]);throw new AssertionError("Expected the old binding path to be absent");}
            catch(UnsatisfiedLinkError e){System.out.println("OBSERVED_OLD_NATIVE_LOAD_ERROR="+e);return;}
        }
        System.load(args[0]);
        if (!NgxConstants.succeeded(NgxNative.nShutdown())) throw new AssertionError("Shutdown before Init must be safe");
        int failedInit = NgxNative.nInitWithProjectId("test", 0, "test", ".", 0, 0, 0, 0, 0, null, NgxConstants.VERSION_API);
        if (NgxConstants.succeeded(failedInit)) throw new AssertionError("Null Vulkan Init must fail safely");
        if (!NgxConstants.succeeded(NgxNative.nShutdown())) throw new AssertionError("Shutdown after rejected Init");
        NgxFeature empty = new NgxFeature();
        int failedCreate = NgxNative.nCreateFeature(0, NgxConstants.FEATURE_SUPER_SAMPLING, 0, empty);
        if (NgxConstants.succeeded(failedCreate) || empty.isValid()) throw new AssertionError("Null Create must fail without a handle");
        empty.close(); empty.close();
        new NgxParameters().close();
        int a=NgxNative.nDestroyParameters(0),b=NgxNative.nReleaseFeature(0);
        if(NgxConstants.succeeded(a)||NgxConstants.succeeded(b))throw new AssertionError("Null handles must fail");
        System.out.println("PASS ACTUAL JNI LOAD: shutdown before Init, null Vulkan Init rejected, shutdown after rejected Init, null Create rejected, empty handle double-close; GPU init/create/evaluate=NOT_RUN; result="+Integer.toUnsignedString(a));
    }
}
