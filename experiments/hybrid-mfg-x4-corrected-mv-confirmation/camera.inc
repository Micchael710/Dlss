struct FixtureCamera {
 static NVSDK_NGX_DLSSG_Opt_Eval_Params options(bool reset){NVSDK_NGX_DLSSG_Opt_Eval_Params o{};o.multiFrameCount=o.multiFrameIndex=1;o.cameraNear=.1f;o.cameraFar=100.f;o.cameraFOV=1.04719755f;o.cameraAspectRatio=float(W)/H;float fy=1/std::tan(o.cameraFOV/2),fx=fy/o.cameraAspectRatio,zz=100.f/99.9f,zw=-10.f/99.9f;
  o.cameraViewToClip[0][0]=fx;o.cameraViewToClip[1][1]=-fy;o.cameraViewToClip[2][2]=zz;o.cameraViewToClip[2][3]=1;o.cameraViewToClip[3][2]=zw;
  o.clipToCameraView[0][0]=1/fx;o.clipToCameraView[1][1]=-1/fy;o.clipToCameraView[2][3]=1/zw;o.clipToCameraView[3][2]=1;o.clipToCameraView[3][3]=-zz/zw;
  for(int i=0;i<4;++i){o.clipToLensClip[i][i]=o.clipToPrevClip[i][i]=o.prevClipToClip[i][i]=1;}
  o.cameraUp[1]=1;o.cameraRight[0]=1;o.cameraFwd[2]=1;o.mvecScale[0]=o.mvecScale[1]=1;o.motionVectorsInvalidValue=std::numeric_limits<float>::max();o.reset=reset;o.cameraMotionIncluded=true;o.depthSubrectSize=o.mvecsSubrectSize=o.hudLessSubrectSize=o.backbufferSubrectSize=o.outputInterpSubrectSize={W,H};return o;
 }
};
