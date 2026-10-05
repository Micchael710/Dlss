#include <windows.h>
#include <psapi.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <nvsdk_ngx_helpers_dlssg_d3d.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <array>
#include <bit>
#include "evidence.inc"
#include "d3d12_session.inc"
#include "camera.inc"

// The session copy retains public direct-x2 setup/ownership only. No full-frame
// upload/readback helpers, swapchain, Vulkan, OpenGL, or production references.
struct Compute {
 D3D12NgxSession& s;
 Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
 Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
 Microsoft::WRL::ComPtr<ID3D12PipelineState> fixture,hybrid,reduction;
 Microsoft::WRL::ComPtr<ID3D12Resource> stats;
 UINT stride{};
 D3D12_CPU_DESCRIPTOR_HANDLE cpu(UINT i){auto h=heap->GetCPUDescriptorHandleForHeapStart();h.ptr+=SIZE_T(i)*stride;return h;}
 D3D12_GPU_DESCRIPTOR_HANDLE gpu(UINT i){auto h=heap->GetGPUDescriptorHandleForHeapStart();h.ptr+=UINT64(i)*stride;return h;}
 explicit Compute(D3D12NgxSession& session,fs::path shaders):s(session){
  D3D12_DESCRIPTOR_RANGE ranges[2]{};
  ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,6,0,0,0};
  ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,4,0,0,0};
  D3D12_ROOT_PARAMETER params[3]{};
  params[0].ParameterType=params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[0].DescriptorTable={1,&ranges[0]};params[1].DescriptorTable={1,&ranges[1]};
  params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  params[2].Constants={0,0,5};
  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;sampler.MaxLOD=D3D12_FLOAT32_MAX;
  D3D12_ROOT_SIGNATURE_DESC desc{3,params,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};
  Microsoft::WRL::ComPtr<ID3DBlob> blob,error;
  s.check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"Serialize root");
  s.check(s.device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"Create root");
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=10;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  s.check(s.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"Create descriptors");
  stride=s.device->GetDescriptorHandleIncrementSize(hd.Type);
  auto make=[&](const char* name,auto& pso){auto bytes=readFile(shaders/(std::string(name)+".cso"));D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={bytes.data(),bytes.size()};s.check(s.device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso)),name);};
  make("FixtureCS",fixture);make("HybridInterpolationCS",hybrid);make("ReductionCS",reduction);
  stats=s.buffer(128,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  for(UINT i=0;i<6;++i)srv(i,nullptr,DXGI_FORMAT_R8G8B8A8_UNORM);
  uav(0,nullptr,DXGI_FORMAT_R8G8B8A8_UNORM);uav(1,nullptr,DXGI_FORMAT_R32_FLOAT);uav(2,nullptr,DXGI_FORMAT_R32G32_FLOAT);
  D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};ud.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;ud.Format=DXGI_FORMAT_R32_TYPELESS;ud.Buffer.NumElements=32;ud.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
  s.device->CreateUnorderedAccessView(stats.Get(),nullptr,&ud,cpu(9));
  s.begin();bind();UINT zero[4]{};s.commands->ClearUnorderedAccessViewUint(gpu(9),cpu(9),stats.Get(),zero,0,nullptr);uavBarrier(stats.Get());s.complete("statistics initialize");
 }
 void srv(UINT i,ID3D12Resource* resource,DXGI_FORMAT format){D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=format;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;s.device->CreateShaderResourceView(resource,&d,cpu(i));}
 void uav(UINT i,ID3D12Resource* resource,DXGI_FORMAT format){D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=format;d.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;s.device->CreateUnorderedAccessView(resource,nullptr,&d,cpu(6+i));}
 void bind(){ID3D12DescriptorHeap* heaps[]={heap.Get()};s.commands->SetDescriptorHeaps(1,heaps);s.commands->SetComputeRootSignature(root.Get());s.commands->SetComputeRootDescriptorTable(0,gpu(0));s.commands->SetComputeRootDescriptorTable(1,gpu(6));}
 void constants(UINT frame,float t,UINT offset){UINT v[]={W,H,frame,std::bit_cast<UINT>(t),offset};s.commands->SetComputeRoot32BitConstants(2,5,v,0);}
 void uavBarrier(ID3D12Resource* resource){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;b.UAV.pResource=resource;s.commands->ResourceBarrier(1,&b);}
 void scene(D3DImage& color,D3DImage& depth,D3DImage& motion,UINT frame){
  uav(0,color.resource.Get(),color.format);uav(1,depth.resource.Get(),depth.format);uav(2,motion.resource.Get(),motion.format);
  s.begin();s.barrier(color,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);s.barrier(depth,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);s.barrier(motion,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  bind();constants(frame,0,0);s.commands->SetPipelineState(fixture.Get());s.commands->Dispatch((W+7)/8,(H+7)/8,1);
  s.barrier(color,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(depth,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(motion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.complete("GPU fixture frame "+std::to_string(frame));
 }
 void sentinel(D3DImage& image,const std::array<float,4>& color){
  uav(0,image.resource.Get(),image.format);s.begin();s.barrier(image,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);bind();s.commands->ClearUnorderedAccessViewFloat(gpu(6),cpu(6),image.resource.Get(),color.data(),0,nullptr);s.barrier(image,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.complete("GPU sentinel");
 }
 void interpolate(D3DImage& a,D3DImage& b,D3DImage& da,D3DImage& db,D3DImage& ma,D3DImage& mb,D3DImage& output,float t){
  D3DImage* inputs[]={&a,&b,&ma,&mb,&da,&db};for(UINT i=0;i<6;++i)srv(i,inputs[i]->resource.Get(),inputs[i]->format);
  uav(0,output.resource.Get(),output.format);s.begin();for(auto input:inputs)s.barrier(*input,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  bind();constants(0,t,0);s.commands->SetPipelineState(hybrid.Get());s.commands->Dispatch((W+7)/8,(H+7)/8,1);
  s.barrier(output,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.complete("Hybrid t="+std::to_string(t));
 }
 void reduce(D3DImage& image,UINT slot){
  srv(0,image.resource.Get(),image.format);s.begin();s.barrier(image,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);bind();constants(0,0,slot*16);s.commands->SetPipelineState(reduction.Get());s.commands->Dispatch((W+7)/8,(H+7)/8,1);uavBarrier(stats.Get());s.complete("GPU fingerprint slot "+std::to_string(slot));
 }
 std::array<UINT,32> readStats(){
  auto readback=s.buffer(128,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);s.begin();s.transition(stats.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);s.commands->CopyBufferRegion(readback.Get(),0,stats.Get(),0,128);s.transition(stats.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);s.complete("128-byte bounded statistics readback");
  void* p{};D3D12_RANGE range{0,128};s.check(readback->Map(0,&range,&p),"Map statistics");std::array<UINT,32> values{};memcpy(values.data(),p,128);D3D12_RANGE empty{0,0};readback->Unmap(0,&empty);return values;
 }
};

int wmain(int argc,wchar_t** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc!=5)return 2;
 fs::path out=fs::absolute(argv[1]),dll=fs::absolute(argv[2]),runtime=fs::absolute(argv[3]),shaders=fs::absolute(argv[4]);
 fs::create_directories(out);vendorLog.open(out/"ngx-callback.log");validationLog.open(out/"d3d12-debug.log");EvidenceRecorder r(out);
 r.str("experiment","WISTERIA_HYBRID_MFG_X4_FRAMESET_GENERATION");r.str("github_start_head","367261845b27eb9135c489ec5f09bf1f8ff6d428");r.raw("nvidia_generated_position","0.5");r.raw("hybrid_generated_positions","[0.25,0.75]");
 r.raw("nvidia_multiframe_count","1");r.raw("nvidia_multiframe_index","1");r.raw("nvidia_evaluate_count","0");r.raw("nvidia_target_evaluate_count","0");r.raw("hybrid_dispatch_count","0");r.raw("cpu_transport_copy_count","0");r.raw("bounded_readback_bytes","0");r.raw("present_executed","false");r.raw("production_quality_interpolation","false");r.str("hybrid_x4_frameset_generation","FAIL");r.str("next_experiment","STOP");r.raw("device_lost","false");r.raw("crash","false");r.str("nvidia_create_result","NOT_RUN");r.str("nvidia_evaluate_result","NOT_RUN");r.str("nvidia_g50_gpu_completion","NOT_RUN");
 for(auto name:{"g25","g50","g75"})for(auto suffix:{"_generated","_changed_from_sentinel","_distinct_from_a_b"})r.raw(std::string(name)+suffix,"false");
 for(auto name:{"g25_distinct_from_g50","g50_distinct_from_g75","g25_distinct_from_g75","temporal_order_valid"})r.raw(name,"false");
 ExternalLoader loader;D3D12NgxSession s(r,runtime);int exit=1;std::string stage="NVIDIA_X2_REGRESSION_IN_NEW_HARNESS";
 try{
  loader.start(r,dll,1);s.initialize();
  auto A=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"A"),B=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"B"),G25=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"G25"),G50=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"G50"),G75=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"G75");
  auto DA=s.image(DXGI_FORMAT_R32_FLOAT,4,"DepthA"),DB=s.image(DXGI_FORMAT_R32_FLOAT,4,"DepthB"),MA=s.image(DXGI_FORMAT_R32G32_FLOAT,8,"MotionA"),MB=s.image(DXGI_FORMAT_R32G32_FLOAT,8,"MotionB");
  auto C=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"warmupColor"),D=s.image(DXGI_FORMAT_R32_FLOAT,4,"warmupDepth"),M=s.image(DXGI_FORMAT_R32G32_FLOAT,8,"warmupMotion");
  Compute compute(s,shaders);compute.scene(A,DA,MA,3);compute.scene(B,DB,MB,4);
  compute.sentinel(G25,{1,0,1,1});compute.reduce(G25,5);
  compute.sentinel(G50,{1,1,1,1});compute.reduce(G50,6);
  compute.sentinel(G75,{0,1,1,1});compute.reduce(G75,7);
  auto code=NVSDK_NGX_D3D12_AllocateParameters(&s.parameters);if(!NVSDK_NGX_SUCCEED(code)||!s.parameters)throw std::runtime_error("NGX AllocateParameters failed");
  NVSDK_NGX_DLSSG_Create_Params cp{};cp.Width=cp.RenderWidth=W;cp.Height=cp.RenderHeight=H;cp.NativeBackbufferFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
  NVSDK_NGX_Parameter_SetUI(s.parameters,NVSDK_NGX_DLSSG_Parameter_Width,W);NVSDK_NGX_Parameter_SetUI(s.parameters,NVSDK_NGX_DLSSG_Parameter_Height,H);
  s.begin();code=NGX_D3D12_CREATE_DLSSG(s.commands.Get(),1,1,&s.feature,s.parameters,&cp);r.str("nvidia_create_result",resultHex(code));if(!NVSDK_NGX_SUCCEED(code)||!s.feature)throw std::runtime_error("NVIDIA CreateFeature failed");s.complete("CreateFeature");
  auto disable=s.buffer(16,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  // Retain four history-building Evaluate calls from the stable fixture. Only
  // the fifth call targets A(f3)/B(f4); all calls remain count=1/index=1.
  for(UINT frame=0;frame<=4;++frame){
   D3DImage *color=&C,*depth=&D,*motion=&M;
   if(frame<3)compute.scene(C,D,M,frame);
   else if(frame==3){color=&A;depth=&DA;motion=&MA;}
   else {color=&B;depth=&DB;motion=&MB;}
   compute.sentinel(G50,{1,1,1,1});s.resetDisable(disable.Get());s.begin();
   s.barrier(*color,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(*depth,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(*motion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(G50,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};ep.pBackbuffer=color->resource.Get();ep.pHudless=color->resource.Get();ep.pDepth=depth->resource.Get();ep.pMVecs=motion->resource.Get();ep.pOutputInterpFrame=G50.resource.Get();ep.pOutputDisableInterpolation=disable.Get();
   auto options=FixtureCamera::options(frame==0);options.cameraViewToClip[1][1]*=-1;options.clipToCameraView[1][1]*=-1;
   code=NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(),s.feature,s.parameters,&ep,&options);r.raw("nvidia_evaluate_count",std::to_string(frame+1));r.str("nvidia_evaluate_result",resultHex(code));if(!NVSDK_NGX_SUCCEED(code))throw std::runtime_error("NVIDIA Evaluate failed");s.complete(frame==4?"x2 target":"warmup");
   if(frame==4){r.raw("nvidia_target_evaluate_count","1");r.raw("nvidia_warmup_evaluate_count","4");r.str("nvidia_g50_gpu_completion","PASS");}
  }
  unsigned status=s.readDisable(disable.Get());r.raw("bounded_readback_bytes","16");r.raw("nvidia_output_disable",std::to_string(status));if(status!=0)throw std::runtime_error("NVIDIA public output status is not enabled");
  stage="HYBRID_SHADER_OUTPUT_NOT_GENERATED";
  compute.interpolate(A,B,DA,DB,MA,MB,G25,0.25f);r.raw("hybrid_dispatch_count","1");r.str("g25_gpu_completion","PASS");
  compute.interpolate(A,B,DA,DB,MA,MB,G75,0.75f);r.raw("hybrid_dispatch_count","2");r.str("g75_gpu_completion","PASS");
  D3DImage* frames[]={&A,&G25,&G50,&G75,&B};const char* names[]={"a","g25","g50","g75","b"};
  for(UINT i=0;i<5;++i)compute.reduce(*frames[i],i);
  auto values=compute.readStats();r.raw("bounded_readback_bytes","144");
  std::array<std::string,8> fingerprints{};std::array<double,5> positions{};
  std::ofstream evidence(out/"frame-fingerprints.txt");
  for(UINT i=0;i<8;++i){std::ostringstream fp;fp<<std::hex<<std::setfill('0')<<std::setw(8)<<values[i*4]<<std::setw(8)<<values[i*4+1];fingerprints[i]=fp.str();if(i<5){r.str(std::string("hash_")+names[i],fp.str());auto mass=values[i*4+3];positions[i]=mass?double(values[i*4+2])/mass:std::numeric_limits<double>::quiet_NaN();r.raw(std::string("position_")+names[i],mass?std::to_string(positions[i]):"null");r.raw(std::string("object_pixels_")+names[i],std::to_string(mass));r.str(std::string("resource_")+names[i],handleText(frames[i]->resource.Get()));evidence<<names[i]<<" fingerprint="<<fp.str()<<" position="<<positions[i]<<" objectPixels="<<mass<<" resource="<<frames[i]->resource.Get()<<'\n';}else{evidence<<"sentinel"<<i-5<<" fingerprint="<<fp.str()<<'\n';}}
  r.str("fingerprint_method","GPU FNV-per-pixel coordinate-keyed XOR + additive reduction, two uint32 words; bounded diagnostic, not cryptographic SHA");
  bool changed=true,distinct=true,ordered=true,resourcesDistinct=true;
  for(UINT i=1;i<=3;++i){bool ch=fingerprints[i]!=fingerprints[i+4];bool ab=fingerprints[i]!=fingerprints[0]&&fingerprints[i]!=fingerprints[4];changed&=ch;distinct&=ab;r.raw(std::string(names[i])+"_changed_from_sentinel",ch?"true":"false");r.raw(std::string(names[i])+"_distinct_from_a_b",ab?"true":"false");r.raw(std::string(names[i])+"_generated",ch&&values[i*4+3]>0?"true":"false");}
  // Sentinel slot mapping: G25->5, G50->6, G75->7.
  for(UINT i=1;i<=3;++i)for(UINT j=i+1;j<=3;++j){bool different=fingerprints[i]!=fingerprints[j];distinct&=different;r.raw(std::string(names[i])+"_distinct_from_"+names[j],different?"true":"false");}
  for(UINT i=0;i<5;++i){ordered&=std::isfinite(positions[i])&&values[i*4+3]>30000&&values[i*4+3]<55000;for(UINT j=i+1;j<5;++j)resourcesDistinct&=frames[i]->resource.Get()!=frames[j]->resource.Get();if(i>0)ordered&=positions[i-1]<positions[i];}
  r.raw("resources_distinct",resourcesDistinct?"true":"false");r.raw("temporal_order_valid",ordered?"true":"false");
  if(!changed)throw std::runtime_error("An output fingerprint equals its sentinel");
  stage="HYBRID_TEMPORAL_REPROJECTION_INVALID";if(!distinct||!ordered||!resourcesDistinct)throw std::runtime_error("Distinctness or temporal order gate failed");
  s.debugMessages();r.raw("d3d12_validation_errors",std::to_string(validationErrors));if(validationErrors)throw std::runtime_error("D3D12 validation errors");
  stage="NONE";r.str("hybrid_x4_frameset_generation","PASS");r.str("next_experiment","HYBRID_X4_PRESENT_SCHEDULER");exit=0;
 }catch(const std::exception& e){r.str("error",e.what());r.event("failure",e.what());}
 if(s.device){auto removed=s.device->GetDeviceRemovedReason();s.removed=FAILED(removed);r.raw("device_lost",s.removed?"true":"false");r.str("device_removed_reason",hrHex(removed));if(s.removed){exit=1;stage="DEVICE_LOST";}}
 s.close();if(exit){r.str("hybrid_x4_frameset_generation","FAIL");r.str("next_experiment","STOP");}r.str("fail_stage",stage);r.raw("process_exit_code",std::to_string(exit));r.flush();std::cout<<"HYBRID_X4_FRAMESET_GENERATION="<<(exit?"FAIL":"PASS")<<" FAIL_STAGE="<<stage<<'\n';return exit;
}
