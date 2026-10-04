package org.ireallywanttosleep.wisteria.dlssg;

import io.homo.superresolution.common.framegeneration.constants.FrameGenerationConstants;
import org.joml.Matrix4f;

/** Explicit OpenGL clip [-1,1] -> D3D12 NGX clip [0,1] metadata mapping. */
final class DlssgMetadata {
    private DlssgMetadata() {}
    static Matrix4f clipBasis() { return new Matrix4f().m22(.5f).m32(.5f); }
    static float[] pack(FrameGenerationConstants c,int rw,int rh,boolean flipMotion) {
        Matrix4f t=clipBasis(), inverse=new Matrix4f(t).invert();
        float[] result=new float[108];
        Matrix4f[] matrices={
            new Matrix4f(t).mul(new Matrix4f().set(c.cameraViewToClip())),
            new Matrix4f().set(c.clipToCameraView()).mul(inverse),
            new Matrix4f(t).mul(new Matrix4f().set(c.clipToLensClip())).mul(inverse),
            new Matrix4f(t).mul(new Matrix4f().set(c.clipToPrevClip())).mul(inverse),
            new Matrix4f(t).mul(new Matrix4f().set(c.prevClipToClip())).mul(inverse)
        };
        // JOML's column-major bytes encode the corresponding row-vector matrix,
        // matching the public D3D12 NGX 4x4 storage (no second transpose).
        for(int i=0;i<5;i++)System.arraycopy(matrices[i].get(new float[16]),0,result,i*16,16);
        float[] scalars={2*c.jitterOffsetX()/rw,2*c.jitterOffsetY()/rh,
            2*c.motionVectorScaleX(),(flipMotion?2:-2)*c.motionVectorScaleY(),
            c.cameraPinholeOffsetX(),c.cameraPinholeOffsetY(),
            c.cameraPosX(),c.cameraPosY(),c.cameraPosZ(),c.cameraUpX(),c.cameraUpY(),c.cameraUpZ(),
            c.cameraRightX(),c.cameraRightY(),c.cameraRightZ(),c.cameraFwdX(),c.cameraFwdY(),c.cameraFwdZ(),
            c.cameraNear(),c.cameraFar(),c.cameraFov(),c.cameraAspectRatio(),c.depthInverted(),
            c.cameraMotionIncluded(),c.motionVectorsInvalidValue(),c.motionVectorsDilated(),
            c.orthographicProjection(),c.minRelativeLinearDepthObjectSeparation()};
        System.arraycopy(scalars,0,result,80,28);
        for(float v:result)if(!Float.isFinite(v))throw new IllegalArgumentException("Non-finite camera metadata");
        if(c.motionVectors3D()!=0)
            throw new IllegalArgumentException("Public first integration requires 2D motion");
        return result;
    }
}
