package io.homo.superresolution.common.upscale.algo.dlss;

/** Maps the existing DLSS UI ratios to public NGX PerfQualityValue enums. */
public final class DlssSrContract {
    private DlssSrContract(){}
    public static int perfQuality(int renderWidth,int displayWidth){
        if(renderWidth<=0||displayWidth<renderWidth)throw new IllegalArgumentException("Invalid DLSS dimensions");
        double ratio=(double)displayWidth/renderWidth;
        double[] ratios={3.0,2.0,1.724,1.5,1.0};
        int[] ngxValues={3,0,1,2,5};
        int best=0;
        for(int i=1;i<ratios.length;i++)if(Math.abs(ratio-ratios[i])<Math.abs(ratio-ratios[best]))best=i;
        return ngxValues[best];
    }
}
