// Offscreen public D3D12/NGX variant. Shares only fixture and evidence utilities.
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <nvsdk_ngx_helpers_dlssg_d3d.h>
using Microsoft::WRL::ComPtr;

static std::string hrHex(HRESULT h){std::ostringstream o;o<<"0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<uint32_t(h);return o.str();}
struct D3DImage {
 ComPtr<ID3D12Resource> resource;
 D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_COMMON;
 DXGI_FORMAT format{};uint32_t bpp{};
};
struct D3D12NgxSession {
 EvidenceRecorder& r;fs::path runtime;
 ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
 ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;
 ComPtr<ID3D12GraphicsCommandList> commands;ComPtr<ID3D12Fence> fence;ComPtr<ID3D12InfoQueue> infoQueue;
 std::vector<ComPtr<ID3D12Resource>> owned;HANDLE completed{};UINT64 fenceValue{};
 NVSDK_NGX_Parameter* parameters{};NVSDK_NGX_Handle* feature{};bool initialized=false,unsafe=false,removed=false;
 explicit D3D12NgxSession(EvidenceRecorder& rec,fs::path rt):r(rec),runtime(rt){}
 void check(HRESULT h,const char* name){r.event("d3d12_result",std::string(name)+"="+hrHex(h));if(FAILED(h)){if(device){auto reason=device->GetDeviceRemovedReason();r.str("device_removed_reason",hrHex(reason));removed=FAILED(reason);r.raw("device_removed",removed?"true":"false");}throw std::runtime_error(std::string(name)+" "+hrHex(h));}}
 void initialize(){
  r.str("if_fail_stage","PRECONDITION");
  ComPtr<ID3D12Debug> debug;auto dh=D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
  r.raw("debug_layer_enabled",SUCCEEDED(dh)?"true":"false");if(SUCCEEDED(dh))debug->EnableDebugLayer();
  check(CreateDXGIFactory2(SUCCEEDED(dh)?DXGI_CREATE_FACTORY_DEBUG:0,IID_PPV_ARGS(&factory)),"CreateDXGIFactory2");
  for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> candidate;auto h=factory->EnumAdapters1(i,&candidate);if(h==DXGI_ERROR_NOT_FOUND)break;check(h,"EnumAdapters1");DXGI_ADAPTER_DESC1 d{};check(candidate->GetDesc1(&d),"GetDesc1");
   auto name=fs::path(d.Description).string();auto luid=hexBytes(reinterpret_cast<const uint8_t*>(&d.AdapterLuid),sizeof(LUID));
   r.event("adapter",name+" vendor="+std::to_string(d.VendorId)+" luid="+luid);
   if(d.VendorId==0x10de&&name.find("3050 Ti")!=std::string::npos&&luid=="4c29010000000000"&&!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){adapter=candidate;r.str("gpu",name);r.raw("vendor_id",std::to_string(d.VendorId));r.raw("device_id",std::to_string(d.DeviceId));r.str("device_luid",luid);break;}
  }
  if(!adapter)throw std::runtime_error("Identified RTX 3050 Ti LUID absent; refusing another adapter");
  check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)),"D3D12CreateDevice");r.str("feature_level","12_0");
  device.As(&infoQueue);
  D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"CreateCommandQueue");
  check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"CreateCommandAllocator");
  check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)),"CreateCommandList");check(commands->Close(),"initial Close");
  check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"CreateFence");completed=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!completed)throw std::runtime_error("CreateEvent failed");
  const auto rt=runtime.wstring();const wchar_t* paths[]={rt.c_str()};NVSDK_NGX_FeatureCommonInfo common{};common.PathListInfo={paths,1};common.LoggingInfo={ngxLog,NVSDK_NGX_LOGGING_LEVEL_VERBOSE,true};
  r.str("if_fail_stage","NGX_INIT");auto code=NVSDK_NGX_D3D12_Init_with_ProjectID("3e6891d2-09ac-4f54-ae8d-f481c3150d4b",NVSDK_NGX_ENGINE_TYPE_CUSTOM,"1.0.0",r.out.c_str(),device.Get(),&common);
  r.str("ngx_init_result",resultHex(code));initialized=NVSDK_NGX_SUCCEED(code);r.str("ngx_init",initialized?"PASS":"FAIL");r.modules("modules_after_ngx_init");if(!initialized)throw std::runtime_error("NGX D3D12 init failed "+resultHex(code));
  NVSDK_NGX_Parameter* caps{};code=NVSDK_NGX_D3D12_GetCapabilityParameters(&caps);r.str("capability_query_result",resultHex(code));
  if(NVSDK_NGX_SUCCEED(code)&&caps){for(auto entry:std::vector<std::pair<std::string,const char*>>{{"available",NVSDK_NGX_Parameter_FrameGeneration_Available},{"needs_updated_driver",NVSDK_NGX_Parameter_FrameGeneration_NeedsUpdatedDriver},{"max",NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax}}){unsigned value{};auto getter=NVSDK_NGX_Parameter_GetUI(caps,entry.second,&value);r.str("adapted_reported_"+entry.first+"_getter_result",resultHex(getter));r.raw("adapted_reported_"+entry.first,NVSDK_NGX_SUCCEED(getter)?std::to_string(value):"null");}int value{};auto getter=NVSDK_NGX_Parameter_GetI(caps,NVSDK_NGX_Parameter_FrameGeneration_FeatureInitResult,&value);r.str("adapted_reported_feature_init_result_getter_result",resultHex(getter));r.raw("adapted_reported_feature_init_result",NVSDK_NGX_SUCCEED(getter)?std::to_string(uint32_t(value)):"null");NVSDK_NGX_D3D12_DestroyParameters(caps);}
 }
 void begin(){check(allocator->Reset(),"allocator Reset");check(commands->Reset(allocator.Get(),nullptr),"command list Reset");}
 void complete(const std::string& tag){
  r.str("if_fail_stage","SYNCHRONIZATION");check(commands->Close(),"command list Close");auto start=std::chrono::steady_clock::now();ID3D12CommandList* lists[]={commands.Get()};queue->ExecuteCommandLists(1,lists);++fenceValue;check(queue->Signal(fence.Get(),fenceValue),"queue Signal");
  r.event("submit",tag+" fenceValue="+std::to_string(fenceValue)+" commandList="+handleText(commands.Get()));
  check(fence->SetEventOnCompletion(fenceValue,completed),"SetEventOnCompletion");auto wait=WaitForSingleObject(completed,15000);r.raw("last_wait_result",std::to_string(wait));
  if(wait!=WAIT_OBJECT_0){unsafe=true;auto reason=device->GetDeviceRemovedReason();removed=FAILED(reason);r.raw("device_removed",removed?"true":"false");r.str("device_removed_reason",hrHex(reason));throw std::runtime_error("Fence timeout/failed wait; no further GPU work");}
  auto reason=device->GetDeviceRemovedReason();r.str("device_removed_reason",hrHex(reason));removed=FAILED(reason);r.raw("device_removed",removed?"true":"false");if(removed){unsafe=true;throw std::runtime_error("Device removed "+hrHex(reason));}
  auto actual=fence->GetCompletedValue();if(actual<fenceValue||actual==UINT64_MAX){unsafe=true;throw std::runtime_error("Invalid completed fence value");}
  r.event("completion",tag+" value="+std::to_string(actual));r.raw("last_completion_value",std::to_string(actual));r.raw("last_submit_wait_cpu_ms",std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()));
  if(tag=="x2 target"){r.raw("target_submit_result","0");r.raw("target_completion_result","0");r.raw("completion_value",std::to_string(actual));r.str("gpu_completion","PASS");r.raw("fg_gpu_time_ms","null");r.str("fg_gpu_time_reason","D3D12 GPU timestamp query not instrumented; CPU submit/wait recorded");}
 }
 ComPtr<ID3D12Resource> buffer(UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){
  D3D12_HEAP_PROPERTIES hp{};hp.Type=type;hp.CreationNodeMask=hp.VisibleNodeMask=1;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;ComPtr<ID3D12Resource> p;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&p)),"CreateCommittedResource buffer");owned.push_back(p);return p;
 }
 D3DImage image(DXGI_FORMAT format,uint32_t bpp,const std::string& label){
  D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};check(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof support),"format support");if(!(support.Support1&D3D12_FORMAT_SUPPORT1_TEXTURE2D)||!(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))throw std::runtime_error("Unsupported fixture format");
  D3DImage im;im.format=format;im.bpp=bpp;D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=W;d.Height=H;d.DepthOrArraySize=d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,im.state,nullptr,IID_PPV_ARGS(&im.resource)),"CreateCommittedResource image");owned.push_back(im.resource);r.event("resource",label+" resource="+handleText(im.resource.Get())+" format="+std::to_string(format)+" extent=1280x720 flags=ALLOW_UNORDERED_ACCESS");return im;
 }
 void barrier(D3DImage& im,D3D12_RESOURCE_STATES next){if(im.state==next){if(next==D3D12_RESOURCE_STATE_UNORDERED_ACCESS){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;b.UAV.pResource=im.resource.Get();commands->ResourceBarrier(1,&b);}return;}D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={im.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,im.state,next};commands->ResourceBarrier(1,&b);im.state=next;}
 void transition(ID3D12Resource* p,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={p,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};commands->ResourceBarrier(1,&b);}
 void resetDisable(ID3D12Resource* p){auto src=buffer(16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* mapped{};D3D12_RANGE empty{0,0};check(src->Map(0,&empty,&mapped),"disable upload Map");memset(mapped,255,16);src->Unmap(0,nullptr);begin();transition(p,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);commands->CopyBufferRegion(p,0,src.Get(),0,16);transition(p,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);complete("disable sentinel upload");}
 unsigned readDisable(ID3D12Resource* p){auto dst=buffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);begin();transition(p,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);commands->CopyBufferRegion(dst.Get(),0,p,0,16);transition(p,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);complete("disable readback");void* mapped{};D3D12_RANGE range{0,16};check(dst->Map(0,&range,&mapped),"disable readback Map");auto value=unsigned(static_cast<uint8_t*>(mapped)[0]);D3D12_RANGE empty{0,0};dst->Unmap(0,&empty);return value;}
 void upload(D3DImage& im,const Bytes& pixels,D3D12_RESOURCE_STATES after){
  if(pixels.size()!=size_t(W)*H*im.bpp)throw std::runtime_error("Fixture byte count mismatch");auto desc=im.resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT rows{};UINT64 rowBytes{},size{};device->GetCopyableFootprints(&desc,0,1,0,&fp,&rows,&rowBytes,&size);auto staging=buffer(size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
  void* mapped{};D3D12_RANGE empty{0,0};check(staging->Map(0,&empty,&mapped),"upload Map");for(UINT y=0;y<H;++y)memcpy(static_cast<uint8_t*>(mapped)+fp.Offset+size_t(y)*fp.Footprint.RowPitch,pixels.data()+size_t(y)*W*im.bpp,W*im.bpp);staging->Unmap(0,nullptr);
  begin();barrier(im,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=im.resource.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=staging.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier(im,after);complete("fixture upload");
 }
 Bytes readback(D3DImage& im,const std::string& label){
  auto desc=im.resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT rows{};UINT64 rowBytes{},size{};device->GetCopyableFootprints(&desc,0,1,0,&fp,&rows,&rowBytes,&size);auto staging=buffer(size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);auto before=im.state;
  begin();barrier(im,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=im.resource.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=staging.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier(im,before);complete(label+" readback");
  Bytes pixels(size_t(W)*H*im.bpp);void* mapped{};D3D12_RANGE range{0,SIZE_T(size)};check(staging->Map(0,&range,&mapped),"readback Map");for(UINT y=0;y<H;++y)memcpy(pixels.data()+size_t(y)*W*im.bpp,static_cast<uint8_t*>(mapped)+fp.Offset+size_t(y)*fp.Footprint.RowPitch,W*im.bpp);D3D12_RANGE empty{0,0};staging->Unmap(0,&empty);save(r.out/(label+".bin"),pixels);r.str(label+"_hash",sha(pixels));if(im.format==DXGI_FORMAT_R8G8B8A8_UNORM)ppm(r.out/(label+".ppm"),pixels);return pixels;
 }
 void debugMessages(){if(!infoQueue)return;for(UINT64 i=0;i<infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T n{};infoQueue->GetMessage(i,nullptr,&n);Bytes bytes(n);auto msg=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());if(SUCCEEDED(infoQueue->GetMessage(i,msg,&n))){validationLog<<msg->ID<<": "<<msg->pDescription<<'\n';if(msg->Severity==D3D12_MESSAGE_SEVERITY_ERROR||msg->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION)++validationErrors;}}validationLog.flush();}
 void close(){if(unsafe||removed){for(auto& p:owned)p.Detach();commands.Detach();allocator.Detach();queue.Detach();fence.Detach();device.Detach();r.event("cleanup","GPU unsafe; no GPU calls, worker exit");return;}if(feature){r.str("release_feature_result",resultHex(NVSDK_NGX_D3D12_ReleaseFeature(feature)));feature=nullptr;}if(parameters){NVSDK_NGX_D3D12_DestroyParameters(parameters);parameters=nullptr;}if(initialized){r.str("ngx_shutdown",resultHex(NVSDK_NGX_D3D12_Shutdown1(device.Get())));initialized=false;}if(completed){CloseHandle(completed);completed=nullptr;}}
};

static int d3d12Worker(fs::path out,fs::path dll,fs::path runtime){
 fs::create_directories(out);vendorLog.open(out/"ngx-callback.log");validationLog.open(out/"d3d12-debug.log");EvidenceRecorder r(out);r.str("scenario","adapted");r.str("api","D3D12");r.raw("generated_count_confirmed","0");r.raw("device_lost","false");r.raw("device_removed","false");r.str("cause_confidence","UNKNOWN");r.str("capability_provenance","REPORTED_POTENTIALLY_HOOKED");
 for(auto key:{"ngx_init","d3d12_createfeature","d3d12_evaluate","gpu_completion","d3d12_output","dlssg_sm86_d3d12_x2"})r.str(key,"NOT_RUN");r.str("d3d12_kernel_create","NOT_OBSERVED");
 ExternalLoader loader;D3D12NgxSession s(r,runtime);int exit=1;
 try{
  loader.start(r,dll);s.initialize();r.str("if_fail_stage","PRECONDITION");if(r.checkpointFailed)throw std::runtime_error("Evidence persistence failed; abort before GPU fixture or feature creation");auto a=SyntheticFixture::color(3),b=SyntheticFixture::color(4),sentinel=SyntheticFixture::sentinel();
  auto A=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"A"),B=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"B"),G=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"G1"),current=s.image(DXGI_FORMAT_R8G8B8A8_UNORM,4,"warmup"),D=s.image(DXGI_FORMAT_R32_FLOAT,4,"depth"),M=s.image(DXGI_FORMAT_R32G32_FLOAT,8,"motion");auto d=SyntheticFixture::depths(4),m=SyntheticFixture::motion(4);
  std::ofstream(out/"fixture.json")<<"{\"api\":\"D3D12\",\"width\":1280,\"height\":720,\"A_frame\":3,\"B_frame\":4,\"translation_pixels\":16,\"mvec_convention\":\"current_to_previous_NDC\",\"mvec_scale\":[1,1],\"camera_fixed\":true,\"jitter\":[0,0],\"warmup_frames\":4,\"color\":\"SDR_RGBA8_UNORM\"}\n";
  s.upload(A,a,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.upload(B,b,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.upload(G,sentinel,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);s.upload(D,d,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.upload(M,m,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  auto ar=s.readback(A,"A"),br=s.readback(B,"B"),dr=s.readback(D,"depth"),mr=s.readback(M,"motion"),sr=s.readback(G,"sentinel");bool validFixture=ar==a&&br==b&&dr==d&&mr==m&&sr==sentinel;r.raw("fixture_readback_valid",validFixture?"true":"false");if(!validFixture)throw std::runtime_error("Fixture roundtrip invalid");
  r.str("if_fail_stage","FEATURE_CREATION");auto code=NVSDK_NGX_D3D12_AllocateParameters(&s.parameters);r.str("allocate_parameters_result",resultHex(code));if(!NVSDK_NGX_SUCCEED(code)||!s.parameters)throw std::runtime_error("AllocateParameters failed");
  NVSDK_NGX_DLSSG_Create_Params cp{};cp.Width=cp.RenderWidth=W;cp.Height=cp.RenderHeight=H;cp.NativeBackbufferFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
  // Public replacement keys documented by the official DLSS-G headers; feature params only.
  NVSDK_NGX_Parameter_SetUI(s.parameters,NVSDK_NGX_DLSSG_Parameter_Width,W);NVSDK_NGX_Parameter_SetUI(s.parameters,NVSDK_NGX_DLSSG_Parameter_Height,H);
  s.begin();r.event("create_begin",handleText(s.commands.Get()));code=NGX_D3D12_CREATE_DLSSG(s.commands.Get(),1,1,&s.feature,s.parameters,&cp);r.str("create_result",resultHex(code));r.str("feature_handle",handleText(s.feature));r.str("d3d12_createfeature",NVSDK_NGX_SUCCEED(code)&&s.feature?"API_SUCCESS_PENDING_COMPLETION":"FAIL");r.modules("modules_after_create");if(!NVSDK_NGX_SUCCEED(code)||!s.feature)throw std::runtime_error("CreateFeature failed "+resultHex(code));s.complete("CreateFeature");r.str("d3d12_createfeature","PASS");
  auto disable=s.buffer(16,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  for(int f=0;f<=4;++f){bool target=f==4;auto real=f==3?&A:(target?&B:&current);if(f<3)s.upload(current,SyntheticFixture::color(f),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.upload(D,SyntheticFixture::depths(f),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.upload(M,SyntheticFixture::motion(f),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.upload(G,sentinel,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   s.resetDisable(disable.Get());s.begin();s.barrier(*real,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(D,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(M,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);s.barrier(G,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);r.str("if_fail_stage","EVALUATE");NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};ep.pBackbuffer=real->resource.Get();ep.pHudless=real->resource.Get();ep.pDepth=D.resource.Get();ep.pMVecs=M.resource.Get();ep.pOutputInterpFrame=G.resource.Get();ep.pOutputDisableInterpolation=disable.Get();auto opts=SyntheticFixture::options(f==0);opts.cameraViewToClip[1][1]*=-1;opts.clipToCameraView[1][1]*=-1;
   r.event("evaluate_begin","realFrameId="+std::to_string(f)+" count=1 index=1 reset="+(f==0?"true":"false")+" timestamp_ms="+std::to_string(f*16.666667)+" deltaMs=16.666667 outputSlot=0 resource="+handleText(G.resource.Get()));
   auto start=std::chrono::steady_clock::now();code=NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(),s.feature,s.parameters,&ep,&opts);r.raw("evaluate_cpu_ms",std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()));r.event("evaluate_result",resultHex(code));r.str("evaluate_result",resultHex(code));if(!NVSDK_NGX_SUCCEED(code)){r.str("d3d12_evaluate","FAIL");throw std::runtime_error("Evaluate failed "+resultHex(code));}r.str("d3d12_evaluate",target?"API_SUCCESS_PENDING_COMPLETION":"WARMUP_SUCCESS");s.complete(target?"x2 target":"warmup");
   if(target){r.str("d3d12_evaluate","PASS");r.str("if_fail_stage","OUTPUT");auto disableValue=s.readDisable(disable.Get());r.raw("output_disable_interpolation",std::to_string(disableValue));auto g=s.readback(G,"G1");auto finalA=s.readback(A,"A_after"),finalB=s.readback(B,"B_after");r.raw("real_inputs_preserved",finalA==a&&finalB==b?"true":"false");s.debugMessages();r.raw("validation_errors",std::to_string(validationErrors));bool valid=disableValue==0&&g!=a&&g!=b&&g!=sentinel&&finalA==a&&finalB==b&&VerdictReducer::output(a,b,g,sentinel,r)&&validationErrors==0;r.str("d3d12_output",valid?"PASS":"FAIL");if(!valid)throw std::runtime_error("Output not a validated temporal interpolation");r.raw("generated_count_confirmed","1");r.str("dlssg_sm86_d3d12_x2","PASS");r.str("if_fail_stage","NONE");r.str("cause_confidence","HIGH");exit=0;}
  }
 }catch(const std::exception& e){r.str("error",e.what());r.event("failure",e.what());r.str("dlssg_sm86_d3d12_x2","FAIL");r.str("cause_confidence","HIGH_STAGE_ONLY; root cause from callback/backend if observed");}
 if(!s.unsafe&&!s.removed)s.debugMessages();r.raw("validation_errors",std::to_string(validationErrors));s.close();r.modules("modules_final");r.flush();return exit;
}
