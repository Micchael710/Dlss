#include <windows.h>
#include <psapi.h>
#include <bcrypt.h>
#include <vulkan/vulkan.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_dlssg_vk.h>
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

namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
constexpr uint32_t W=1280,H=720,DX=16;
static std::mutex logMutex;
static std::ofstream vendorLog,validationLog;
static std::string jsonQuote(const std::string& s) {
 std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;
 else if(c=='\n')o<<"\\n";else if(c=='\r')o<<"\\r";else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c);else o<<c;}return o.str()+'"';
}
static std::string hexBytes(const uint8_t* p,size_t n){std::ostringstream o;o<<std::hex<<std::setfill('0');for(size_t i=0;i<n;++i)o<<std::setw(2)<<unsigned(p[i]);return o.str();}
static std::string resultHex(NVSDK_NGX_Result r){std::ostringstream o;o<<"0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<uint32_t(r);return o.str();}
template<class T>static std::string handleText(T h){std::ostringstream o;o<<h;return o.str();}
static void NVSDK_CONV ngxLog(const char* s,NVSDK_NGX_Logging_Level,NVSDK_NGX_Feature){std::lock_guard l(logMutex);vendorLog<<s<<'\n';vendorLog.flush();}
static unsigned validationErrors=0;
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* d,void*) {
 std::lock_guard l(logMutex);validationLog<<severity<<": "<<(d->pMessage?d->pMessage:"")<<'\n';validationLog.flush();if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)++validationErrors;return VK_FALSE;
}
static Bytes readFile(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read "+p.string());return Bytes(std::istreambuf_iterator<char>(f),{});}
static void save(const fs::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("Cannot write "+p.string());}
static std::string sha(const Bytes& b){BCRYPT_ALG_HANDLE a{};BCRYPT_HASH_HANDLE h{};DWORD n{},got{};
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA provider");
 BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&n),sizeof n,&got,0);Bytes obj(n),digest(32);
 if(BCryptCreateHash(a,&h,obj.data(),n,nullptr,0,0)<0||BCryptHashData(h,const_cast<PUCHAR>(b.data()),ULONG(b.size()),0)<0||BCryptFinishHash(h,digest.data(),32,0)<0)throw std::runtime_error("SHA operation");
 BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);return hexBytes(digest.data(),32);
}
static void ppm(const fs::path& p,const Bytes& rgba){std::ofstream f(p,std::ios::binary);f<<"P6\n"<<W<<' '<<H<<"\n255\n";for(size_t i=0;i<rgba.size();i+=4)f.write(reinterpret_cast<const char*>(rgba.data()+i),3);}
struct EvidenceRecorder{
 fs::path out;std::map<std::string,std::string> data;std::ofstream events;std::ostringstream bootstrap;bool checkpointFailed=false;
 explicit EvidenceRecorder(fs::path p):out(p),events(p/"events.jsonl"){}
 void raw(std::string k,std::string v){data[k]=v;flush();}
 void str(std::string k,std::string v){raw(k,jsonQuote(v));}
 void event(std::string type,std::string detail){events<<"{\"timestamp_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<",\"pid\":"<<GetCurrentProcessId()<<",\"type\":"<<jsonQuote(type)<<",\"detail\":"<<jsonQuote(detail)<<"}\n";events.flush();}
 void flush(){
  auto serialize=[&](const fs::path& p){std::ofstream f(p);f<<'{';bool first=true;for(auto& [k,v]:data){f<<(first?"":",")<<jsonQuote(k)<<':'<<v;first=false;}f<<bootstrap.str()<<"}\n";f.flush();return bool(f);};
  auto tmp=out/"worker-result.tmp";DWORD error=ERROR_WRITE_FAULT;
  if(serialize(tmp))for(int attempt=0;attempt<5;++attempt){if(MoveFileExW(tmp.c_str(),(out/"worker-result.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return;error=GetLastError();if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_ACCESS_DENIED)break;Sleep(5);}
  // Evidence failures must not throw again while recording the original exception.
  checkpointFailed=true;data["evidence_checkpoint_failed"]="true";data["evidence_checkpoint_win32_error"]=std::to_string(error);
  std::ofstream(out/"evidence-io.log",std::ios::app)<<"checkpoint replacement failed Win32="<<error<<'\n';
  serialize(out/"worker-recovery.json");
 }
 void modules(std::string key){HMODULE ms[1024];DWORD n{};std::string list="[";if(EnumProcessModules(GetCurrentProcess(),ms,sizeof ms,&n)){for(DWORD i=0;i<std::min<DWORD>(n/sizeof(HMODULE),1024);++i){wchar_t p[32768];if(GetModuleFileNameExW(GetCurrentProcess(),ms[i],p,32768)){if(list.size()>1)list+=',';list+=jsonQuote(fs::path(p).string());}}}raw(key,list+"]");}
};
struct ExternalLoader{
 HMODULE module{};
 void start(EvidenceRecorder& r,const fs::path& dll){
  r.str("if_fail_stage","PRECONDITION");auto hash=sha(readFile(dll));r.str("external_dll_sha256",hash);r.str("external_dll_path",dll.string());
  if(hash!="c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838")throw std::runtime_error("Not the identified sdli 0.3.5 module");
  r.modules("modules_before_external");std::ofstream ini(dll.parent_path()/"dlssg_sm86.ini");
  ini<<"[General]\nEnabled=1\n[FrameGeneration]\nOptimized=0\nMaxGeneratedFrames=1\n[Compatibility]\nRouter=SM86\nKernelImage=Auto\nSpoofArchToGame=0\n[Logging]\nLevel=3\nDirectory="<<(r.out/"backend").string()<<"\n[Runtime]\nMode=Bundled\nCacheDirectory="<<(r.out/"bundle-cache").string()<<'\n';ini.close();if(!ini)throw std::runtime_error("INI write failed");
  r.str("if_fail_stage","LOADER");r.event("external_load_begin",dll.string());module=LoadLibraryW(dll.c_str());auto err=GetLastError();r.str("external_module",handleText(module));r.raw("loader_win32_error",module?"0":std::to_string(err));if(!module)throw std::runtime_error("External LoadLibrary failed Win32="+std::to_string(err));
  r.str("public_initializer","NONE_SDLI_LOAD_CONTRACT");r.str("external_loader_ready","LOADED");r.str("backend_install_observation","NOT_YET_OBSERVED");r.modules("modules_after_external");r.event("external_load_complete",handleText(module));
 }
 // No destructor: hooks and module remain resident until worker process exit.
};
struct VulkanNgxSession{
 EvidenceRecorder& r;fs::path output,runtime;HMODULE vulkan{};VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t queueIndex{};VkCommandPool pool{};VkCommandBuffer cmd{};VkFence fence{};
 PFN_vkGetInstanceProcAddr gipa{};PFN_vkGetDeviceProcAddr gdpa{};PFN_vkDestroyInstance destroyInstance{};PFN_vkDestroyDevice destroyDevice{};VkDebugUtilsMessengerEXT messenger{};VkQueryPool timestamps{};uint32_t timestampBits{};float timestampPeriod{};uint64_t submitSequence{};bool initialized=false,deviceLost=false,unsafe=false;NVSDK_NGX_Parameter* parameters{};NVSDK_NGX_Handle* feature{};
 VulkanNgxSession(EvidenceRecorder& e,fs::path rt):r(e),output(e.out),runtime(rt){}
 template<class F>F fn(const char* n){auto p=reinterpret_cast<F>(gipa(instance,n));if(!p)throw std::runtime_error(std::string("Missing ")+n);return p;}
 void checked(VkResult v,const char* n){r.event("vulkan_result",std::string(n)+"="+std::to_string(v));if(v==VK_ERROR_DEVICE_LOST){deviceLost=true;r.raw("device_lost","true");}if(v!=VK_SUCCESS)throw std::runtime_error(std::string(n)+" VkResult="+std::to_string(v));}
 template<class F>F load(PFN_vkGetInstanceProcAddr p,VkInstance i,const char* n){auto f=reinterpret_cast<F>(p(i,n));if(!f)throw std::runtime_error(std::string("Missing ")+n);return f;}
 void initialize();
 void begin(){checked(fn<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(cmd,0),"vkResetCommandBuffer");VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;checked(fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(cmd,&bi),"vkBeginCommandBuffer");if(timestamps){fn<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(cmd,timestamps,0,2);fn<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,0);}}
 void complete(const std::string& tag){
  r.str("if_fail_stage","synchronization");
  if(timestamps)fn<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,1);
  checked(fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(cmd),"vkEndCommandBuffer");
  checked(fn<PFN_vkResetFences>("vkResetFences")(device,1,&fence),"vkResetFences");
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cmd;
  auto start=std::chrono::steady_clock::now();++submitSequence;
  auto v=fn<PFN_vkQueueSubmit>("vkQueueSubmit")(queue,1,&si,fence);
  r.event("submit",tag+" sequence="+std::to_string(submitSequence)+" result="+std::to_string(v)+" commandBuffer="+handleText(cmd)+" fence="+handleText(fence));
  r.raw("last_submit_result",std::to_string(v));checked(v,"vkQueueSubmit");
  v=fn<PFN_vkWaitForFences>("vkWaitForFences")(device,1,&fence,VK_TRUE,15000000000ull);
  r.event("completion",tag+" sequence="+std::to_string(submitSequence)+" result="+std::to_string(v));
  r.raw("last_completion_result",std::to_string(v));
  auto cpuMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  r.raw("last_submit_wait_cpu_ms",std::to_string(cpuMs));
  if(v!=VK_SUCCESS)unsafe=true;checked(v,"vkWaitForFences");
  if(tag=="x2 target"){
   r.raw("target_submit_result","0");r.raw("target_completion_result","0");r.raw("target_submit_sequence",std::to_string(submitSequence));
   r.str("completion_primitive","BINARY_FENCE");r.raw("completion_value","null");r.raw("target_submit_wait_cpu_ms",std::to_string(cpuMs));
   r.raw("fg_gpu_time_ms","null");
   if(timestamps){uint64_t ticks[2]{};auto q=fn<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(device,timestamps,0,2,sizeof ticks,ticks,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT);r.raw("timestamp_query_result",std::to_string(q));if(q==VK_ERROR_DEVICE_LOST)checked(q,"timestamp readback");if(q==VK_SUCCESS){auto mask=timestampBits==64?~uint64_t(0):((uint64_t(1)<<timestampBits)-1);auto delta=(ticks[1]-ticks[0])&mask;r.raw("fg_gpu_time_ms",std::to_string(double(delta)*timestampPeriod/1e6));r.str("fg_gpu_time_scope","target command buffer including host barriers");}}
  }
 }
 void close(){if(deviceLost||unsafe){r.event("cleanup","GPU unsafe; no further calls; process exit reclaims handles");return;}if(feature){r.str("release_feature",resultHex(NVSDK_NGX_VULKAN_ReleaseFeature(feature)));feature=nullptr;}if(parameters)NVSDK_NGX_VULKAN_DestroyParameters(parameters);if(initialized)r.str("ngx_shutdown",resultHex(NVSDK_NGX_VULKAN_Shutdown1(device)));if(timestamps)fn<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(device,timestamps,nullptr);if(fence)fn<PFN_vkDestroyFence>("vkDestroyFence")(device,fence,nullptr);if(pool)fn<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(device,pool,nullptr);if(device)destroyDevice(device,nullptr);if(messenger)fn<PFN_vkDestroyDebugUtilsMessengerEXT>("vkDestroyDebugUtilsMessengerEXT")(instance,messenger,nullptr);if(instance)destroyInstance(instance,nullptr);}
};
#include "session_setup.inc"
struct Buffer{VkBuffer b{};VkDeviceMemory mem{};VkDeviceSize size{};bool coherent{};};
struct Image{VkImage image{};VkImageView view{};VkDeviceMemory mem{};VkFormat format{};uint32_t bytesPerPixel{};bool general=false;NVSDK_NGX_Resource_VK ngx{};};
struct GpuReadback{
 VulkanNgxSession& s;std::vector<Buffer> buffers;std::vector<Image> images;
 explicit GpuReadback(VulkanNgxSession& session):s(session){}
 uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred=0){VkPhysicalDeviceMemoryProperties mp{};s.fn<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(s.physical,&mp);for(int pass=0;pass<2;++pass)for(uint32_t i=0;i<mp.memoryTypeCount;++i)if((bits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&required)==required&&(!pass?(mp.memoryTypes[i].propertyFlags&preferred)==preferred:true))return i;throw std::runtime_error("No memory type");}
 Buffer buffer(size_t n,VkBufferUsageFlags usage){Buffer b;b.size=n;VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=n;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;s.checked(s.fn<PFN_vkCreateBuffer>("vkCreateBuffer")(s.device,&ci,nullptr,&b.b),"vkCreateBuffer");VkMemoryRequirements mr{};s.fn<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(s.device,b.b,&mr);VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=memoryType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);VkPhysicalDeviceMemoryProperties mp{};s.fn<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(s.physical,&mp);b.coherent=(mp.memoryTypes[ai.memoryTypeIndex].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;s.checked(s.fn<PFN_vkAllocateMemory>("vkAllocateMemory")(s.device,&ai,nullptr,&b.mem),"vkAllocateMemory buffer");s.checked(s.fn<PFN_vkBindBufferMemory>("vkBindBufferMemory")(s.device,b.b,b.mem,0),"vkBindBufferMemory");buffers.push_back(b);return b;}
 void write(Buffer b,const Bytes& data){void* p{};s.checked(s.fn<PFN_vkMapMemory>("vkMapMemory")(s.device,b.mem,0,VK_WHOLE_SIZE,0,&p),"map upload");memcpy(p,data.data(),data.size());if(!b.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=b.mem;range.size=VK_WHOLE_SIZE;s.checked(s.fn<PFN_vkFlushMappedMemoryRanges>("vkFlushMappedMemoryRanges")(s.device,1,&range),"flush upload");}s.fn<PFN_vkUnmapMemory>("vkUnmapMemory")(s.device,b.mem);}
 Bytes read(Buffer b){void* p{};s.checked(s.fn<PFN_vkMapMemory>("vkMapMemory")(s.device,b.mem,0,VK_WHOLE_SIZE,0,&p),"map readback");if(!b.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=b.mem;range.size=VK_WHOLE_SIZE;s.checked(s.fn<PFN_vkInvalidateMappedMemoryRanges>("vkInvalidateMappedMemoryRanges")(s.device,1,&range),"invalidate readback");}Bytes data(size_t(b.size));memcpy(data.data(),p,data.size());s.fn<PFN_vkUnmapMemory>("vkUnmapMemory")(s.device,b.mem);return data;}
 Image image(VkFormat fmt,uint32_t bpp,const std::string& label){Image im;im.format=fmt;im.bytesPerPixel=bpp;VkFormatProperties fp{};s.fn<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties")(s.physical,fmt,&fp);auto need=VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;if((fp.optimalTilingFeatures&need)!=need)throw std::runtime_error("Fixture unsupported image format");VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=fmt;ci.extent={W,H,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;s.checked(s.fn<PFN_vkCreateImage>("vkCreateImage")(s.device,&ci,nullptr,&im.image),"vkCreateImage");VkMemoryRequirements mr{};s.fn<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(s.device,im.image,&mr);VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=memoryType(mr.memoryTypeBits,0,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);s.checked(s.fn<PFN_vkAllocateMemory>("vkAllocateMemory")(s.device,&ai,nullptr,&im.mem),"image memory");s.checked(s.fn<PFN_vkBindImageMemory>("vkBindImageMemory")(s.device,im.image,im.mem,0),"vkBindImageMemory");VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=im.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=fmt;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};s.checked(s.fn<PFN_vkCreateImageView>("vkCreateImageView")(s.device,&vi,nullptr,&im.view),"vkCreateImageView");im.ngx.Type=NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;im.ngx.ReadWrite=true;im.ngx.Resource.ImageViewInfo={im.view,im.image,vi.subresourceRange,fmt,W,H};images.push_back(im);s.r.event("resource",label+" image="+handleText(im.image)+" view="+handleText(im.view)+" format="+std::to_string(fmt));return im;}
 void barrier(Image& im,VkAccessFlags dst,VkPipelineStageFlags stage){VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=im.general?(VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT):0;b.dstAccessMask=dst;b.oldLayout=im.general?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=im.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};s.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(s.cmd,im.general?VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,stage,0,0,nullptr,0,nullptr,1,&b);im.general=true;}
 void upload(Image& im,const Bytes& data){if(data.size()!=size_t(W)*H*im.bytesPerPixel)throw std::runtime_error("Fixture byte size");auto b=buffer(data.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);write(b,data);s.begin();barrier(im,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);VkBufferImageCopy c{};c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};c.imageExtent={W,H,1};s.fn<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(s.cmd,b.b,im.image,VK_IMAGE_LAYOUT_GENERAL,1,&c);barrier(im,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);s.complete("fixture upload");}
 Bytes readback(Image& im,const std::string& label){auto b=buffer(size_t(W)*H*im.bytesPerPixel,VK_BUFFER_USAGE_TRANSFER_DST_BIT);s.begin();barrier(im,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);VkBufferImageCopy c{};c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};c.imageExtent={W,H,1};s.fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(s.cmd,im.image,VK_IMAGE_LAYOUT_GENERAL,b.b,1,&c);VkBufferMemoryBarrier bm{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};bm.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;bm.dstAccessMask=VK_ACCESS_HOST_READ_BIT;bm.srcQueueFamilyIndex=bm.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bm.buffer=b.b;bm.size=VK_WHOLE_SIZE;s.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(s.cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&bm,0,nullptr);s.complete(label+" readback");auto data=read(b);save(s.output/(label+".bin"),data);s.r.str(label+"_hash",sha(data));if(im.bytesPerPixel==4&&im.format==VK_FORMAT_R8G8B8A8_UNORM)ppm(s.output/(label+".ppm"),data);return data;}
 void close(){if(s.deviceLost||s.unsafe)return;for(auto& im:images){s.fn<PFN_vkDestroyImageView>("vkDestroyImageView")(s.device,im.view,nullptr);s.fn<PFN_vkDestroyImage>("vkDestroyImage")(s.device,im.image,nullptr);s.fn<PFN_vkFreeMemory>("vkFreeMemory")(s.device,im.mem,nullptr);}for(auto& b:buffers){s.fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(s.device,b.b,nullptr);s.fn<PFN_vkFreeMemory>("vkFreeMemory")(s.device,b.mem,nullptr);}}
};
struct SyntheticFixture{
 static float depth(float z){return 100.f/99.9f-10.f/(99.9f*z);}
 static Bytes color(double f){Bytes b(size_t(W)*H*4);int left=300+int(std::lround(DX*f));for(int y=0;y<int(H);++y)for(int x=0;x<int(W);++x){size_t i=(size_t(y)*W+x)*4;bool obj=x>=left&&x<left+192&&y>=248&&y<472;int tex=((obj?(x-left):x)/12+y/12)&1;b[i]=uint8_t(obj?190+tex*40:20+tex*12);b[i+1]=uint8_t(obj?70+tex*30:65+tex*12);b[i+2]=uint8_t(obj?25+tex*20:115+tex*12);b[i+3]=255;}return b;}
 static Bytes sentinel(){Bytes b(size_t(W)*H*4);for(size_t i=0;i<b.size();i+=4){bool t=((i/4)%W/16+(i/4/W)/16)&1;b[i]=255;b[i+1]=uint8_t(t?255:0);b[i+2]=255;b[i+3]=255;}return b;}
 static Bytes depths(int f){std::vector<float> p(size_t(W)*H);int left=300+int(DX)*f;for(int y=0;y<int(H);++y)for(int x=0;x<int(W);++x)p[size_t(y)*W+x]=depth(x>=left&&x<left+192&&y>=248&&y<472?2.f:10.f);Bytes b(p.size()*4);memcpy(b.data(),p.data(),b.size());return b;}
 static Bytes motion(int f){std::vector<float> p(size_t(W)*H*2,0.f);int left=300+int(DX)*f;for(int y=248;y<472;++y)for(int x=left;x<left+192;++x)p[(size_t(y)*W+x)*2]=-2.f*DX/W;Bytes b(p.size()*4);memcpy(b.data(),p.data(),b.size());return b;}
 static NVSDK_NGX_DLSSG_Opt_Eval_Params options(bool reset){NVSDK_NGX_DLSSG_Opt_Eval_Params o{};o.multiFrameCount=o.multiFrameIndex=1;o.cameraNear=.1f;o.cameraFar=100.f;o.cameraFOV=1.04719755f;o.cameraAspectRatio=float(W)/H;float fy=1/std::tan(o.cameraFOV/2),fx=fy/o.cameraAspectRatio,zz=100.f/99.9f,zw=-10.f/99.9f;
  o.cameraViewToClip[0][0]=fx;o.cameraViewToClip[1][1]=-fy;o.cameraViewToClip[2][2]=zz;o.cameraViewToClip[2][3]=1;o.cameraViewToClip[3][2]=zw;
  o.clipToCameraView[0][0]=1/fx;o.clipToCameraView[1][1]=-1/fy;o.clipToCameraView[2][3]=1/zw;o.clipToCameraView[3][2]=1;o.clipToCameraView[3][3]=-zz/zw;
  for(int i=0;i<4;++i){o.clipToLensClip[i][i]=o.clipToPrevClip[i][i]=o.prevClipToClip[i][i]=1;}
  o.cameraUp[1]=1;o.cameraRight[0]=1;o.cameraFwd[2]=1;o.mvecScale[0]=o.mvecScale[1]=1;o.motionVectorsInvalidValue=std::numeric_limits<float>::max();o.reset=reset;o.cameraMotionIncluded=true;o.depthSubrectSize=o.mvecsSubrectSize=o.hudLessSubrectSize=o.backbufferSubrectSize=o.outputInterpSubrectSize={W,H};return o;
 }
};
struct VerdictReducer{
 static bool output(const Bytes& a,const Bytes& b,const Bytes& g,const Bytes& sentinel,EvidenceRecorder& r){
  if(a.size()!=size_t(W)*H*4||b.size()!=a.size()||g.size()!=a.size()||sentinel.size()!=a.size())return false;
  size_t changedA=0,changedB=0,sentinelPixels=0,red=0,backgroundBad=0,backgroundTotal=0;double redX=0,redY=0,totalError=0;
  auto ideal=SyntheticFixture::color(3.5);
  for(uint32_t y=0;y<H;++y)for(uint32_t x=0;x<W;++x){size_t i=(size_t(y)*W+x)*4;
   changedA+=memcmp(g.data()+i,a.data()+i,3)!=0;changedB+=memcmp(g.data()+i,b.data()+i,3)!=0;sentinelPixels+=memcmp(g.data()+i,sentinel.data()+i,3)==0;
   int err=0;for(size_t c=0;c<3;++c)err+=std::abs(int(g[i+c])-int(ideal[i+c]));totalError+=err;
   if(x<330||x>575||y<230||y>490){++backgroundTotal;backgroundBad+=err>24;}
   if(g[i]>150&&g[i+1]<140&&g[i+2]<90){++red;redX+=x;redY+=y;}
  }
  double center=red?redX/red:0,centerY=red?redY/red:0;double ca=300+3*DX+95.5,cb=300+4*DX+95.5;
  r.raw("changed_pixels_vs_A",std::to_string(changedA));r.raw("changed_pixels_vs_B",std::to_string(changedB));r.raw("sentinel_matching_pixels",std::to_string(sentinelPixels));r.raw("red_object_pixels",std::to_string(red));r.raw("generated_object_centroid_x",std::to_string(center));r.raw("generated_object_centroid_y",std::to_string(centerY));r.raw("rgb_mae_vs_ideal",std::to_string(totalError/(W*H*3)));r.raw("background_bad_pixels",std::to_string(backgroundBad));
  bool valid=changedA>1024&&changedB>1024&&sentinelPixels<100&&red>30000&&red<55000&&center>ca+2&&center<cb-2&&std::abs(centerY-359.5)<3&&backgroundBad<backgroundTotal/100&&totalError/(W*H*3)<12;
  r.raw("fixture_temporal_content_valid",valid?"true":"false");return valid;
 }
};
static int selfTest(const fs::path& output){
 fs::create_directories(output);EvidenceRecorder r(output);auto a=SyntheticFixture::color(3),b=SyntheticFixture::color(4),sentinel=SyntheticFixture::sentinel();
 auto require=[](bool pass,const char* why){if(!pass)throw std::runtime_error(why);};
 require(!VerdictReducer::output(a,b,a,sentinel,r),"duplicate A accepted");require(!VerdictReducer::output(a,b,b,sentinel,r),"duplicate B accepted");require(!VerdictReducer::output(a,b,sentinel,sentinel,r),"sentinel accepted");
 auto pixel=a;pixel[0]^=1;require(!VerdictReducer::output(a,b,pixel,sentinel,r),"one pixel accepted");require(!VerdictReducer::output(a,b,Bytes(a.size(),0),sentinel,r),"black accepted");
 auto corrupt=SyntheticFixture::color(3.5);for(size_t i=0;i<corrupt.size();i+=4)if(corrupt[i]<100)corrupt[i+2]=0;require(!VerdictReducer::output(a,b,corrupt,sentinel,r),"corrupt background accepted");
 require(VerdictReducer::output(a,b,SyntheticFixture::color(3.5),sentinel,r),"coherent midpoint rejected");
 auto o=SyntheticFixture::options(false);for(int i=0;i<4;++i)for(int j=0;j<4;++j){float x=0;for(int k=0;k<4;++k)x+=o.cameraViewToClip[i][k]*o.clipToCameraView[k][j];require(std::abs(x-(i==j?1.f:0.f))<1e-4f,"camera inverse mismatch");}
 require(SyntheticFixture::depth(.1f)<1e-5f&&std::abs(SyntheticFixture::depth(100.f)-1.f)<1e-5f,"depth projection mismatch");
 auto lockedDir=output/"locked-checkpoint";fs::create_directories(lockedDir);EvidenceRecorder locked(lockedDir);locked.str("before","valid");auto held=CreateFileW((lockedDir/"worker-result.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);require(held!=INVALID_HANDLE_VALUE,"cannot establish checkpoint lock test");locked.str("while_locked","recovery");require(locked.checkpointFailed&&fs::exists(lockedDir/"worker-recovery.json"),"locked checkpoint did not preserve evidence");CloseHandle(held);locked.flush();
 r.str("self_test","PASS");r.raw("gpu_used","false");r.raw("community_loaded","false");r.raw("checkpoint_lock_recovery_test","true");std::cout<<"CPU verdict, camera and locked-checkpoint tests PASS\n";return 0;
}
static int worker(fs::path output,fs::path dll,fs::path runtime){
 fs::create_directories(output);vendorLog.open(output/"ngx-callback.log");validationLog.open(output/"vulkan-validation.log");EvidenceRecorder r(output);r.str("scenario","adapted");r.raw("generated_count_confirmed","0");r.raw("device_lost","false");r.str("dlssg_sm86_vulkan_x2","NOT_RUN");for(auto k:{"ngx_init","vulkan_createfeature","vulkan_evaluate","gpu_completion","vulkan_output"})r.str(k,"NOT_RUN");r.str("vulkan_kernel_create","NOT_OBSERVED");r.str("cause_confidence","UNKNOWN");r.raw("adapted_original_available","null");r.raw("adapted_original_max","null");r.str("capability_provenance","REPORTED_POTENTIALLY_HOOKED");
 ExternalLoader external;VulkanNgxSession s(r,runtime);GpuReadback gpu(s);int exit=1;
 try{
  external.start(r,dll);s.initialize();r.str("if_fail_stage","PRECONDITION");
  auto A=gpu.image(VK_FORMAT_R8G8B8A8_UNORM,4,"A"),B=gpu.image(VK_FORMAT_R8G8B8A8_UNORM,4,"B"),G=gpu.image(VK_FORMAT_R8G8B8A8_UNORM,4,"G1"),current=gpu.image(VK_FORMAT_R8G8B8A8_UNORM,4,"warmup"),D=gpu.image(VK_FORMAT_R32_SFLOAT,4,"depth"),M=gpu.image(VK_FORMAT_R32G32_SFLOAT,8,"motion");auto a=SyntheticFixture::color(3),b=SyntheticFixture::color(4),sentinel=SyntheticFixture::sentinel(),d=SyntheticFixture::depths(4),m=SyntheticFixture::motion(4);
  std::ofstream(output/"fixture.json")<<"{\"width\":1280,\"height\":720,\"A_frame\":3,\"B_frame\":4,\"translation_pixels\":16,\"mvec_convention\":\"current_to_previous_NDC\",\"mvec_scale\":[1,1],\"camera_fixed\":true,\"jitter\":[0,0],\"depth_near\":0.1,\"depth_far\":100,\"color\":\"SDR_RGBA8_UNORM\",\"warmup_frames\":4}\n";
  gpu.upload(A,a);gpu.upload(B,b);gpu.upload(G,sentinel);gpu.upload(D,d);gpu.upload(M,m);
  bool fixture=gpu.readback(A,"A")==a&&gpu.readback(B,"B")==b&&gpu.readback(D,"depth")==d&&gpu.readback(M,"motion")==m&&gpu.readback(G,"sentinel")==sentinel;r.raw("fixture_readback_valid",fixture?"true":"false");if(!fixture)throw std::runtime_error("Fixture roundtrip invalid");
  r.str("if_fail_stage","feature creation");auto alloc=NVSDK_NGX_VULKAN_AllocateParameters(&s.parameters);r.str("allocate_parameters_result",resultHex(alloc));if(!NVSDK_NGX_SUCCEED(alloc)||!s.parameters)throw std::runtime_error("Feature parameter allocation failed");
  NVSDK_NGX_DLSSG_Create_Params cp{};cp.Width=cp.RenderWidth=W;cp.Height=cp.RenderHeight=H;cp.NativeBackbufferFormat=VK_FORMAT_R8G8B8A8_UNORM;s.begin();r.event("create_begin",handleText(s.cmd));auto cr=NGX_VK_CREATE_DLSSG(s.cmd,1,1,&s.feature,s.parameters,&cp);r.str("create_result",resultHex(cr));r.str("feature_handle",handleText(s.feature));r.str("vulkan_createfeature",NVSDK_NGX_SUCCEED(cr)&&s.feature?"API_SUCCESS_PENDING_COMPLETION":"FAIL");r.modules("modules_after_create");if(!NVSDK_NGX_SUCCEED(cr)||!s.feature)throw std::runtime_error("CreateFeature failed "+resultHex(cr));s.complete("CreateFeature");r.str("vulkan_createfeature","PASS");
  Buffer disabled=gpu.buffer(4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);NVSDK_NGX_Resource_VK disableResource{};disableResource.Type=NVSDK_NGX_RESOURCE_VK_TYPE_VK_BUFFER;disableResource.ReadWrite=true;disableResource.Resource.BufferInfo={disabled.b,4};
  for(int f=0;f<=4;++f){bool target=f==4;Image* real=f==3?&A:(target?&B:&current);if(f<3)gpu.upload(current,SyntheticFixture::color(f));gpu.upload(D,SyntheticFixture::depths(f));gpu.upload(M,SyntheticFixture::motion(f));gpu.upload(G,sentinel);gpu.write(disabled,Bytes(4,255));s.begin();gpu.barrier(*real,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);gpu.barrier(D,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);gpu.barrier(M,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);gpu.barrier(G,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);r.str("if_fail_stage","evaluate");NVSDK_NGX_VK_DLSSG_Eval_Params ep{};ep.pBackbuffer=&real->ngx;ep.pHudless=&real->ngx;ep.pDepth=&D.ngx;ep.pMVecs=&M.ngx;ep.pOutputInterpFrame=&G.ngx;ep.pOutputDisableInterpolation=&disableResource;auto opts=SyntheticFixture::options(f==0);r.event("evaluate_begin","realFrameId="+std::to_string(f)+" count=1 index=1 reset="+(f==0?"true":"false")+" timestamp_ms="+std::to_string(f*16.666667)+" deltaMs=16.666667 outputSlot=0 image="+handleText(G.image));auto er=NGX_VK_EVALUATE_DLSSG(s.cmd,s.feature,s.parameters,&ep,&opts);r.event("evaluate_result",resultHex(er));r.str("evaluate_result",resultHex(er));if(!NVSDK_NGX_SUCCEED(er)){r.str("vulkan_evaluate","FAIL");throw std::runtime_error("Evaluate failed "+resultHex(er));}r.str("vulkan_evaluate",target?"API_SUCCESS_PENDING_COMPLETION":"WARMUP_SUCCESS");s.complete(target?"x2 target":"warmup");if(target){r.str("vulkan_evaluate","PASS");r.str("gpu_completion","PASS");
    VkBufferMemoryBarrier bm{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};bm.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;bm.dstAccessMask=VK_ACCESS_HOST_READ_BIT;bm.srcQueueFamilyIndex=bm.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;bm.buffer=disabled.b;bm.size=4;s.begin();s.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(s.cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&bm,0,nullptr);s.complete("disable readback");auto flag=gpu.read(disabled);r.raw("output_disable_interpolation",std::to_string(flag[0]));r.str("if_fail_stage","output");auto g=gpu.readback(G,"G1");bool valid=flag[0]==0&&g!=sentinel&&g!=a&&g!=b&&VerdictReducer::output(a,b,g,sentinel,r)&&validationErrors==0;r.raw("validation_errors",std::to_string(validationErrors));r.str("vulkan_output",valid?"PASS":"FAIL");if(!valid)throw std::runtime_error("Output not a validated temporal interpolation");r.raw("generated_count_confirmed","1");r.str("dlssg_sm86_vulkan_x2","PASS");r.str("if_fail_stage","NONE");r.str("cause_confidence","HIGH");exit=0;}
  }
 }catch(const std::exception& e){r.str("error",e.what());r.event("failure",e.what());r.str("dlssg_sm86_vulkan_x2","FAIL");if(r.data["cause_confidence"]==jsonQuote("UNKNOWN"))r.str("cause_confidence","STAGE_CONFIRMED_ROOT_CAUSE_PENDING_LOGS");}
 try{if(!s.unsafe&&!s.deviceLost&&s.feature){NVSDK_NGX_VULKAN_ReleaseFeature(s.feature);s.feature=nullptr;}gpu.close();s.close();}catch(const std::exception& e){r.event("cleanup_error",e.what());}
 r.raw("validation_errors",std::to_string(validationErrors));r.modules("modules_final");r.flush();return exit;
}
#include "d3d12_worker.inc"
static std::wstring winQuote(std::wstring s){std::wstring o=L"\"";unsigned slashes=0;for(wchar_t c:s){if(c==L'\\'){++slashes;continue;}if(c==L'"'){o.append(slashes*2+1,L'\\');o+=c;}else{o.append(slashes,L'\\');o+=c;}slashes=0;}o.append(slashes*2,L'\\');return o+L"\"";}
int wmain(int argc,wchar_t** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc==3&&std::wstring(argv[1])==L"--self-test")return selfTest(fs::absolute(argv[2]));
 if(argc==5&&std::wstring(argv[1])==L"--worker")return worker(fs::absolute(argv[2]),fs::absolute(argv[3]),fs::absolute(argv[4]));
 if(argc==6&&std::wstring(argv[1])==L"--worker"&&std::wstring(argv[5])==L"D3D12")return d3d12Worker(fs::absolute(argv[2]),fs::absolute(argv[3]),fs::absolute(argv[4]));
 if(argc!=4&&argc!=5){std::cerr<<"Usage: dlssg_external_harness RUN_DIRECTORY IDENTIFIED_DLL STOCK_RUNTIME_DIRECTORY [D3D12]\n";return 2;}
 bool d3d=argc==5&&std::wstring(argv[4])==L"D3D12";if(argc==5&&!d3d)return 2;
 fs::path out=fs::absolute(argv[1]);fs::create_directories(out);wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);
 std::wstring command=winQuote(exe)+L" --worker "+winQuote(out.wstring())+L" "+winQuote(fs::absolute(argv[2]).wstring())+L" "+winQuote(fs::absolute(argv[3]).wstring())+(d3d?L" D3D12":L"");
 STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
 if(!CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,out.c_str(),&si,&pi)){std::ofstream(out/"coordinator.json")<<"{\"spawn_error\":"<<GetLastError()<<"}\n";return 2;}
 auto wait=WaitForSingleObject(pi.hProcess,180000);if(wait==WAIT_TIMEOUT){TerminateProcess(pi.hProcess,124);WaitForSingleObject(pi.hProcess,5000);}DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);
 std::ofstream(out/"coordinator.json")<<"{\"worker_pid\":"<<pi.dwProcessId<<",\"worker_exit_code\":"<<code<<",\"timeout\":"<<(wait==WAIT_TIMEOUT?"true":"false")<<",\"attempts\":1,\"api\":"<<jsonQuote(d3d?"D3D12":"Vulkan")<<"}\n";
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return int(code);
}
