package org.ireallywanttosleep.wisteria.dlssg;

import io.homo.superresolution.common.framegeneration.constants.*;
import org.joml.*;

/** CPU-only geometry and new JNI linkage checks; never initializes NGX or a GPU. */
public final class DlssgContractTest {
    private static void near(float a,float b){if(java.lang.Math.abs(a-b)>2e-4f)throw new AssertionError(a+" != "+b);}
    public static void main(String[] args){
        Matrix4f projection=new Matrix4f().perspective(1.1f,16f/9,.1f,100f);
        var camera=new FGConstantsBuilder.CameraFrame(1,projection,new Matrix4f(),new Vector3d(),
            new Vector3f(0,1,0),new Vector3f(1,0,0),new Vector3f(0,0,-1),.1f,100f,1.1f,16f/9);
        var constants=FGConstantsBuilder.build(camera,null,true,true,false,.25f,-.25f,1,1);
        float[] packed=DlssgMetadata.pack(constants,1280,720,false);
        float[] matrix=new float[16];System.arraycopy(packed,0,matrix,0,16);
        Matrix4f converted=new Matrix4f().set(matrix);
        near(converted.transformProject(new Vector3f(0,0,-.1f)).z,0);
        near(converted.transformProject(new Vector3f(0,0,-100f)).z,1);
        near(converted.m11(),projection.m11());
        System.arraycopy(packed,16,matrix,0,16);
        if(!converted.mul(new Matrix4f().set(matrix),new Matrix4f()).equals(new Matrix4f(),2e-4f))throw new AssertionError("Camera inverse mismatch");
        // A pixel moving down by 10 has previous-current top-UV motion -10/H,
        // which becomes positive D3D clip-Y motion +20/H. The unflipped GL
        // producer has the opposite raw Y sign and uses the opposite scale.
        near((-10f/720)*packed[83],20f/720);
        float[] borrowed=DlssgMetadata.pack(constants,1280,720,true);
        near((10f/720)*borrowed[83],20f/720);
        near(packed[80],.5f/1280);near(packed[81],-.5f/720);
        float saved=constants.cameraViewToClip()[0];float[] exposed=constants.cameraViewToClip();exposed[0]=123;
        near(constants.cameraViewToClip()[0],saved);
        for(int i=2;i<5;i++){Matrix4f identity=new Matrix4f().set(java.util.Arrays.copyOfRange(packed,i*16,(i+1)*16));if(!identity.equals(new Matrix4f(),2e-4f))throw new AssertionError("Reset history not identity");}
        System.load(java.nio.file.Path.of(args[0]).toAbsolutePath().toString());
        if(DlssgBridge.abi()!=1)throw new AssertionError("JNI ABI mismatch");
        System.out.println("PASS: perspective near/far, camera inverse, DX Y convention, borrowed/owned motion, pixel jitter, immutable constants, reset matrices, new JNI ABI/linkage. GPU/community module not initialized.");
    }
}
