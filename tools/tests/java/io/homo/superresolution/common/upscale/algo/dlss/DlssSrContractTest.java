package io.homo.superresolution.common.upscale.algo.dlss;
public final class DlssSrContractTest {
    public static void main(String[] args){
        int[][] dimensions={{640,1920,3},{960,1920,0},{1113,1920,1},{1280,1920,2},{1920,1920,5},{495,854,1}};
        for(var d:dimensions)if(DlssSrContract.perfQuality(d[0],d[1])!=d[2])throw new AssertionError();
        try{DlssSrContract.perfQuality(0,1920);throw new AssertionError();}catch(IllegalArgumentException expected){}
        try{DlssSrContract.perfQuality(1920,960);throw new AssertionError();}catch(IllegalArgumentException expected){}
        System.out.println("PASS CPU: existing DLSS preset ratios, rounded dimensions, invalid extent rejection; no GPU capability simulation");
    }
}
