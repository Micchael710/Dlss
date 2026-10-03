// Public capability gate only; no NGX linkage, community loader, queue or dispatch.
#include <windows.h>
#include <vulkan/vulkan.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <chrono>
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
static std::ofstream validation,debuglog,events;
static unsigned vkErrors=0,dxErrors=0;
static std::string quote(const std::string& s){std::string o="\"";for(char c:s){if(c=='"'||c=='\\')o+='\\';if(c=='\n'){o+="\\n";continue;}o+=c;}return o+'"';}
static std::string hex(const void* p,size_t n){std::ostringstream o;o<<std::hex<<std::setfill('0');auto b=static_cast<const unsigned char*>(p);for(size_t i=0;i<n;++i)o<<std::setw(2)<<unsigned(b[i]);return o.str();}
static std::map<std::string,std::string> fields;
static void put(const std::string& k,const std::string& v){fields[k]=quote(v);}
static void raw(const std::string& k,const std::string& v){fields[k]=v;}
static void event(const std::string& s){events<<"{\"steady_ns\":"<<std::chrono::steady_clock::now().time_since_epoch().count()<<",\"detail\":"<<quote(s)<<"}\n";events.flush();}
static void write(const fs::path& p){std::ofstream f(p);f<<"{\n";bool first=true;for(auto&[k,v]:fields){if(!first)f<<",\n";first=false;f<<quote(k)<<":"<<v;}f<<"\n}\n";if(!f)throw std::runtime_error("Evidence write failed");}
static void hr(HRESULT x,const char* where){event(std::string(where)+" HRESULT="+std::to_string(uint32_t(x)));if(FAILED(x))throw std::runtime_error(std::string(where)+" HRESULT="+std::to_string(uint32_t(x)));}
static void vk(VkResult x,const char* where){event(std::string(where)+" VkResult="+std::to_string(x));if(x!=VK_SUCCESS)throw std::runtime_error(std::string(where)+" VkResult="+std::to_string(x));}
static VKAPI_ATTR VkBool32 VKAPI_CALL callback(VkDebugUtilsMessageSeverityFlagBitsEXT s,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* d,void*){validation<<s<<":"<<d->pMessage<<'\n';validation.flush();if(s&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)++vkErrors;return VK_FALSE;}
template<class T>static T load(PFN_vkGetInstanceProcAddr g,VkInstance i,const char* name){auto p=reinterpret_cast<T>(g(i,name));if(!p)throw std::runtime_error(std::string("Missing public Vulkan API ")+name);return p;}
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;fs::path out=fs::absolute(argv[1]);fs::create_directories(out);
 validation.open(out/"vulkan-validation.log");debuglog.open(out/"d3d12-debug.log");events.open(out/"events.jsonl");
 put("if_fail_stage","CAPABILITY");put("cross_api_interop","NOT_RUN");put("same_gpu","NOT_RUN");raw("bridge_cpu_copy_count","0");raw("dlssg_executed","false");raw("minecraft_launched","false");
 for(auto k:{"vulkan_to_d3d12_resource_share","d3d12_to_vulkan_resource_share","cross_api_gpu_sync","roundtrip"})put(k,"NOT_RUN");
 VkInstance instance{};VkDebugUtilsMessengerEXT messenger{};PFN_vkGetInstanceProcAddr g{};ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> info;HANDLE rh{},fh{};int exit=1;
 try{
  auto library=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library)throw std::runtime_error("System Vulkan loader absent");g=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library,"vkGetInstanceProcAddr"));
  uint32_t n{};auto enumerate=load<PFN_vkEnumerateInstanceLayerProperties>(g,{},"vkEnumerateInstanceLayerProperties");vk(enumerate(&n,nullptr),"layer count");std::vector<VkLayerProperties> layers(n);vk(enumerate(&n,layers.data()),"layers");
  bool haveLayer=std::any_of(layers.begin(),layers.end(),[](auto&l){return std::string(l.layerName)=="VK_LAYER_KHRONOS_validation";});raw("vulkan_validation_layer_enabled",haveLayer?"true":"false");
  auto ext=load<PFN_vkEnumerateInstanceExtensionProperties>(g,{},"vkEnumerateInstanceExtensionProperties");vk(ext(nullptr,&n,nullptr),"extension count");std::vector<VkExtensionProperties> extensions(n);vk(ext(nullptr,&n,extensions.data()),"extensions");bool haveDebug=std::any_of(extensions.begin(),extensions.end(),[](auto&e){return std::string(e.extensionName)=="VK_EXT_debug_utils";});
  const char* layer="VK_LAYER_KHRONOS_validation";const char* de="VK_EXT_debug_utils";VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Isolated cross API capability gate";app.apiVersion=VK_API_VERSION_1_2;
  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;if(haveLayer){ci.enabledLayerCount=1;ci.ppEnabledLayerNames=&layer;}if(haveDebug){ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=&de;}
  vk(load<PFN_vkCreateInstance>(g,{},"vkCreateInstance")(&ci,nullptr,&instance),"CreateInstance");
  if(haveDebug){VkDebugUtilsMessengerCreateInfoEXT d{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};d.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;d.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;d.pfnUserCallback=callback;vk(load<PFN_vkCreateDebugUtilsMessengerEXT>(g,instance,"vkCreateDebugUtilsMessengerEXT")(instance,&d,nullptr,&messenger),"debug messenger");}
  auto en=load<PFN_vkEnumeratePhysicalDevices>(g,instance,"vkEnumeratePhysicalDevices");vk(en(instance,&n,nullptr),"GPU count");std::vector<VkPhysicalDevice> devices(n);vk(en(instance,&n,devices.data()),"GPUs");VkPhysicalDevice physical{};VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  for(auto p:devices){props.pNext=&ids;load<PFN_vkGetPhysicalDeviceProperties2>(g,instance,"vkGetPhysicalDeviceProperties2")(p,&props);if(props.properties.vendorID==0x10de&&hex(ids.deviceUUID,16)=="01895b66d1ca454d88788dd21fdef638"&&ids.deviceLUIDValid&&hex(ids.deviceLUID,8)=="4c29010000000000"){physical=p;break;}}
  if(!physical){put("if_fail_stage","ADAPTER_MISMATCH");throw std::runtime_error("Exact Vulkan UUID/LUID absent");}put("vulkan_gpu",props.properties.deviceName);put("vulkan_uuid",hex(ids.deviceUUID,16));put("vulkan_luid",hex(ids.deviceLUID,8));raw("driver_raw",std::to_string(props.properties.driverVersion));
  auto dex=load<PFN_vkEnumerateDeviceExtensionProperties>(g,instance,"vkEnumerateDeviceExtensionProperties");vk(dex(physical,nullptr,&n,nullptr),"device extension count");std::vector<VkExtensionProperties> des(n);vk(dex(physical,nullptr,&n,des.data()),"device extensions");
  bool extensionsOkay=true;for(auto name:{"VK_KHR_external_memory_win32","VK_KHR_external_semaphore_win32"}){bool present=std::any_of(des.begin(),des.end(),[&](auto&e){return std::string(e.extensionName)==name;});raw(name,present?"true":"false");extensionsOkay&=present;}
  struct Cap{bool imported=false,exported=false;};std::map<std::string,Cap> caps;
  for(auto pair:std::vector<std::pair<std::string,VkExternalMemoryHandleTypeFlagBits>>{{"d3d12_resource",VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT},{"d3d12_heap",VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT}}){
   VkPhysicalDeviceExternalImageFormatInfo ei{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};ei.handleType=pair.second;VkPhysicalDeviceImageFormatInfo2 fi{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};fi.pNext=&ei;fi.format=VK_FORMAT_R8G8B8A8_UNORM;fi.type=VK_IMAGE_TYPE_2D;fi.tiling=VK_IMAGE_TILING_OPTIMAL;fi.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   VkExternalImageFormatProperties ep{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};VkImageFormatProperties2 ip{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};ip.pNext=&ep;auto result=load<PFN_vkGetPhysicalDeviceImageFormatProperties2>(g,instance,"vkGetPhysicalDeviceImageFormatProperties2")(physical,&fi,&ip);auto& m=ep.externalMemoryProperties;auto prefix=pair.first;
   raw(prefix+"_query_result",std::to_string(result));raw(prefix+"_externalMemoryFeatures",std::to_string(m.externalMemoryFeatures));raw(prefix+"_compatibleHandleTypes",std::to_string(m.compatibleHandleTypes));raw(prefix+"_exportFromImportedHandleTypes",std::to_string(m.exportFromImportedHandleTypes));
   auto& c=caps[prefix];c.imported=result==0&&(m.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT);c.exported=result==0&&(m.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT);raw(prefix+"_importable",c.imported?"true":"false");raw(prefix+"_exportable",c.exported?"true":"false");raw(prefix+"_dedicated_only",(m.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)?"true":"false");
  }
  VkPhysicalDeviceExternalSemaphoreInfo si{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO};si.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;VkExternalSemaphoreProperties sp{VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};load<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>(g,instance,"vkGetPhysicalDeviceExternalSemaphoreProperties")(physical,&si,&sp);
  raw("fence_externalSemaphoreFeatures",std::to_string(sp.externalSemaphoreFeatures));raw("fence_compatibleHandleTypes",std::to_string(sp.compatibleHandleTypes));raw("fence_exportFromImportedHandleTypes",std::to_string(sp.exportFromImportedHandleTypes));bool fenceImport=sp.externalSemaphoreFeatures&VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT,fenceExport=sp.externalSemaphoreFeatures&VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT;raw("fence_importable",fenceImport?"true":"false");raw("fence_exportable",fenceExport?"true":"false");
  ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))){debug->EnableDebugLayer();raw("d3d12_debug_layer_enabled","true");}else raw("d3d12_debug_layer_enabled","false");ComPtr<IDXGIFactory4> factory;hr(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"DXGI factory");ComPtr<IDXGIAdapter1> chosen;
  for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;auto result=factory->EnumAdapters1(i,&a);if(result==DXGI_ERROR_NOT_FOUND)break;hr(result,"adapter enumerate");DXGI_ADAPTER_DESC1 d{};hr(a->GetDesc1(&d),"adapter desc");if(d.VendorId==0x10de&&hex(&d.AdapterLuid,8)==hex(ids.deviceLUID,8)&&!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){chosen=a;put("d3d12_gpu",fs::path(d.Description).string());put("d3d12_luid",hex(&d.AdapterLuid,8));break;}}
  if(!chosen){put("if_fail_stage","ADAPTER_MISMATCH");throw std::runtime_error("Matching D3D12 adapter absent");}put("same_gpu","PASS");hr(D3D12CreateDevice(chosen.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)),"D3D12 device");device.As(&info);
  D3D12_FEATURE_DATA_FORMAT_SUPPORT support{DXGI_FORMAT_R8G8B8A8_UNORM};hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof support),"D3D12 format support");raw("d3d12_format_support1",std::to_string(support.Support1));
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=rd.Height=256;rd.DepthOrArraySize=rd.MipLevels=1;rd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;rd.SampleDesc.Count=1;ComPtr<ID3D12Resource> resource,opened;
  hr(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_SHARED,&rd,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&resource)),"D3D12 shared resource");hr(device->CreateSharedHandle(resource.Get(),nullptr,GENERIC_ALL,nullptr,&rh),"resource CreateSharedHandle");hr(device->OpenSharedHandle(rh,IID_PPV_ARGS(&opened)),"resource OpenSharedHandle");put("d3d12_shared_resource_support","PASS");CloseHandle(rh);rh=nullptr;event("Resource handle application-owned; closed after OpenSharedHandle; both resource references live");
  ComPtr<ID3D12Fence> fence,openedFence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&fence)),"shared fence");hr(device->CreateSharedHandle(fence.Get(),nullptr,GENERIC_ALL,nullptr,&fh),"fence CreateSharedHandle");hr(device->OpenSharedHandle(fh,IID_PPV_ARGS(&openedFence)),"fence OpenSharedHandle");CloseHandle(fh);fh=nullptr;put("d3d12_shared_fence_support","PASS");
  bool memory=(caps["d3d12_resource"].imported&&caps["d3d12_resource"].exported)||(caps["d3d12_heap"].imported&&caps["d3d12_heap"].exported);
  raw("bidirectional_memory_handle_available",memory?"true":"false");raw("bidirectional_fence_handle_available",fenceImport&&fenceExport?"true":"false");
  if(!memory||!fenceImport||!fenceExport||!extensionsOkay)throw std::runtime_error("CAPABILITY gate: literal Vulkan-export/D3D12-open route requires an exportable and importable D3D12 memory handle plus external fence support");
  put("capability_gate","PASS");put("cross_api_interop","PENDING_PHASE_B");put("if_fail_stage","NONE");exit=0;
 }catch(const std::exception& e){put("error",e.what());put("cross_api_interop","FAIL");put("cause_confidence","HIGH_OBSERVED_STAGE_AND_PUBLIC_CAPABILITY_FLAGS");event(e.what());}
 if(device){raw("device_removed",FAILED(device->GetDeviceRemovedReason())?"true":"false");raw("device_removed_reason",std::to_string(uint32_t(device->GetDeviceRemovedReason())));}
 if(info){for(UINT64 i=0;i<info->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T n{};info->GetMessage(i,nullptr,&n);std::vector<char> b(n);auto m=reinterpret_cast<D3D12_MESSAGE*>(b.data());if(SUCCEEDED(info->GetMessage(i,m,&n))){debuglog<<m->ID<<":"<<m->pDescription<<'\n';if(m->Severity==D3D12_MESSAGE_SEVERITY_ERROR||m->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION)++dxErrors;}}}
 raw("vulkan_validation_errors",std::to_string(vkErrors));raw("d3d12_debug_errors",std::to_string(dxErrors));if(vkErrors||dxErrors){put("if_fail_stage","VALIDATION");put("cross_api_interop","FAIL");exit=1;}
 if(rh)CloseHandle(rh);if(fh)CloseHandle(fh);if(messenger)load<PFN_vkDestroyDebugUtilsMessengerEXT>(g,instance,"vkDestroyDebugUtilsMessengerEXT")(instance,messenger,nullptr);if(instance)load<PFN_vkDestroyInstance>(g,instance,"vkDestroyInstance")(instance,nullptr);
 write(out/"capabilities.json");write(out/"result.json");return exit;
}
