struct FixtureCamera {
 static NVSDK_NGX_DLSSG_Opt_Eval_Params options(bool reset, bool cameraMotionIncluded = true, float scaleX = 1.0f, float scaleY = 1.0f) {
  NVSDK_NGX_DLSSG_Opt_Eval_Params o{};
  o.multiFrameCount = o.multiFrameIndex = 1;
  o.cameraNear = 0.1f;
  o.cameraFar = 100.0f;
  o.cameraFOV = 1.04719755f;
  o.cameraAspectRatio = float(W) / H;
  float fy = 1.0f / std::tan(o.cameraFOV / 2.0f), fx = fy / o.cameraAspectRatio, zz = 100.0f / 99.9f, zw = -10.0f / 99.9f;
  o.cameraViewToClip[0][0] = fx;
  o.cameraViewToClip[1][1] = -fy;
  o.cameraViewToClip[2][2] = zz;
  o.cameraViewToClip[2][3] = 1.0f;
  o.cameraViewToClip[3][2] = zw;
  o.clipToCameraView[0][0] = 1.0f / fx;
  o.clipToCameraView[1][1] = -1.0f / fy;
  o.clipToCameraView[2][3] = 1.0f / zw;
  o.clipToCameraView[3][2] = 1.0f;
  o.clipToCameraView[3][3] = -zz / zw;
  for (int i = 0; i < 4; ++i) {
   o.clipToLensClip[i][i] = o.clipToPrevClip[i][i] = o.prevClipToClip[i][i] = 1.0f;
  }
  o.cameraUp[1] = 1.0f;
  o.cameraRight[0] = 1.0f;
  o.cameraFwd[2] = 1.0f;
  o.mvecScale[0] = scaleX;
  o.mvecScale[1] = scaleY;
  o.motionVectorsInvalidValue = std::numeric_limits<float>::max();
  o.reset = reset;
  o.cameraMotionIncluded = cameraMotionIncluded;
  o.depthSubrectSize = o.mvecsSubrectSize = o.hudLessSubrectSize = o.backbufferSubrectSize = o.outputInterpSubrectSize = {W, H};
  return o;
 }
};
