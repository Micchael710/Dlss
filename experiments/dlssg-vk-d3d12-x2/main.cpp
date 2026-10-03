// Reuse the closed-gate public loader, fixture, reducer and D3D12 NGX session.
// Historical entry points are compiled but never called by this executable.
#define wmain HistoricalHarnessEntryNotCalled
#include "../dlssg-external-harness/harness.cpp"
#undef wmain
#include <array>

static unsigned vkErrors = 0;
static std::ofstream vkLog;
static VKAPI_ATTR VkBool32 VKAPI_CALL bridgeCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
 VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void*) {
 std::lock_guard lock(logMutex);
 vkLog << severity << ": " << (message->pMessage ? message->pMessage : "") << '\n';
 vkLog.flush(); if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++vkErrors;
 return VK_FALSE;
}

static uint32_t intersectionType(uint32_t image, uint32_t handle,
 const VkPhysicalDeviceMemoryProperties& memory, VkMemoryPropertyFlags required) {
 for (uint32_t i=0; i<memory.memoryTypeCount; ++i)
  if ((image & handle & (1u<<i)) && (memory.memoryTypes[i].propertyFlags & required)==required) return i;
 throw std::runtime_error("No compatible DEVICE_LOCAL image/Win32 memory type intersection");
}

struct SharedImage {
 std::string label; D3DImage dx; VkImage vk{}; VkDeviceMemory memory{};
 VkFormat format{}; bool local=false, initialized=false;
};

// Vulkan is used only for shared-resource IO. This initializer calls no NGX Vulkan API.
struct SharedBridge : VulkanNgxSession {
 D3D12NgxSession& dx;
 GpuReadback buffers;
 VkPhysicalDeviceMemoryProperties memoryProperties{};
 std::array<SharedImage,5> images;
 VkCommandBuffer input{}, consume{};
 VkSemaphore sharedTimeline{};
 ComPtr<ID3D12Fence> sharedFence;
 std::vector<HANDLE> unclosedHandles;
 bool pending=false;
 std::ostringstream resourceMap, handles;
 bool firstResource=true, firstHandle=true;
 SharedBridge(EvidenceRecorder& recorder, D3D12NgxSession& d)
  : VulkanNgxSession(recorder, {}), dx(d), buffers(*this) { resourceMap<<'['; handles<<'['; }
 void stage(const char* name) { r.str("if_fail_stage",name); r.event("stage",name); }
 void initializeInterop() {
  stage("RESOURCE_CREATION");
  vulkan=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  if(!vulkan) throw std::runtime_error("System Vulkan loader absent");
  gipa=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkan,"vkGetInstanceProcAddr"));
  auto enumLayers=load<PFN_vkEnumerateInstanceLayerProperties>(gipa,{},"vkEnumerateInstanceLayerProperties");
  uint32_t count{}; checked(enumLayers(&count,nullptr),"layer count");
  std::vector<VkLayerProperties> layers(count); checked(enumLayers(&count,layers.data()),"layers");
  const char* validation="VK_LAYER_KHRONOS_validation";
  bool haveLayer=std::any_of(layers.begin(),layers.end(),[&](auto& p){return std::string(p.layerName)==validation;});
  r.raw("vulkan_validation_available",haveLayer?"true":"false");
  auto enumExtensions=load<PFN_vkEnumerateInstanceExtensionProperties>(gipa,{},"vkEnumerateInstanceExtensionProperties");
  checked(enumExtensions(nullptr,&count,nullptr),"instance extension count");
  std::vector<VkExtensionProperties> extensions(count); checked(enumExtensions(nullptr,&count,extensions.data()),"instance extensions");
  const char* debug="VK_EXT_debug_utils";
  bool haveDebug=std::any_of(extensions.begin(),extensions.end(),[&](auto& p){return std::string(p.extensionName)==debug;});
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName="Vulkan D3D12 DLSSG Vulkan isolated x2"; app.apiVersion=VK_API_VERSION_1_2;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app;
  if(haveLayer){ici.enabledLayerCount=1;ici.ppEnabledLayerNames=&validation;}
  if(haveDebug){ici.enabledExtensionCount=1;ici.ppEnabledExtensionNames=&debug;}
  checked(load<PFN_vkCreateInstance>(gipa,{},"vkCreateInstance")(&ici,nullptr,&instance),"instance create");
  destroyInstance=fn<PFN_vkDestroyInstance>("vkDestroyInstance");
  if(haveDebug){VkDebugUtilsMessengerCreateInfoEXT m{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
   m.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   m.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   m.pfnUserCallback=bridgeCallback; checked(fn<PFN_vkCreateDebugUtilsMessengerEXT>("vkCreateDebugUtilsMessengerEXT")(instance,&m,nullptr,&messenger),"debug messenger");}
  auto enumGpu=fn<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices"); checked(enumGpu(instance,&count,nullptr),"GPU count");
  std::vector<VkPhysicalDevice> gpus(count); checked(enumGpu(instance,&count,gpus.data()),"GPUs");
  VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
  VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES}; id.pNext=&driver;
  VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; props.pNext=&id;
  for(auto gpu:gpus){fn<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(gpu,&props);
   if(props.properties.vendorID==0x10de && id.deviceLUIDValid &&
     hexBytes(id.deviceUUID,16)=="01895b66d1ca454d88788dd21fdef638" &&
     hexBytes(id.deviceLUID,8)=="4c29010000000000"){physical=gpu;break;}}
  if(!physical || r.data["device_luid"]!=jsonQuote(hexBytes(id.deviceLUID,8))) throw std::runtime_error("Same exact UUID/LUID GPU required");
  r.str("vulkan_gpu",props.properties.deviceName); r.str("vulkan_uuid",hexBytes(id.deviceUUID,16));
  r.str("vulkan_luid",hexBytes(id.deviceLUID,8)); r.str("d3d12_gpu",fs::path(L"NVIDIA GeForce RTX 3050 Ti Laptop GPU").string());
  r.str("d3d12_luid",hexBytes(id.deviceLUID,8)); r.str("driver_info",driver.driverInfo);
  r.raw("driver_raw",std::to_string(props.properties.driverVersion)); r.str("same_gpu","PASS");
  timestampPeriod=props.properties.limits.timestampPeriod;
  fn<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical,&memoryProperties);
  auto enumDevExt=fn<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
  checked(enumDevExt(physical,nullptr,&count,nullptr),"device extension count"); extensions.resize(count);
  checked(enumDevExt(physical,nullptr,&count,extensions.data()),"device extensions");
  const char* names[]={"VK_KHR_external_memory_win32","VK_KHR_external_semaphore_win32"};
  for(auto name:names) if(std::none_of(extensions.begin(),extensions.end(),[&](auto& p){return std::string(p.extensionName)==name;})) throw std::runtime_error(std::string("Missing ")+name);
  VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
  VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext=&timeline;
  fn<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(physical,&features);
  if(!timeline.timelineSemaphore) throw std::runtime_error("Timeline unavailable");
  fn<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(physical,&count,nullptr);
  std::vector<VkQueueFamilyProperties> queues(count); fn<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(physical,&count,queues.data());
  queueIndex=0; while(queueIndex<count && !(queues[queueIndex].queueFlags&VK_QUEUE_GRAPHICS_BIT)) ++queueIndex;
  if(queueIndex==count) throw std::runtime_error("Graphics queue unavailable"); timestampBits=queues[queueIndex].timestampValidBits;
  float priority=1; VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; q.queueFamilyIndex=queueIndex;q.queueCount=1;q.pQueuePriorities=&priority;
  VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dc.pNext=&timeline; dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&q;dc.enabledExtensionCount=2;dc.ppEnabledExtensionNames=names;
  checked(fn<PFN_vkCreateDevice>("vkCreateDevice")(physical,&dc,nullptr,&device),"device create"); destroyDevice=fn<PFN_vkDestroyDevice>("vkDestroyDevice");
  fn<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(device,queueIndex,0,&queue);
  VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=queueIndex;pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  checked(fn<PFN_vkCreateCommandPool>("vkCreateCommandPool")(device,&pc,nullptr,&pool),"pool create");
  VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=2;
  VkCommandBuffer cs[2];checked(fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(device,&ca,cs),"command allocation");input=cs[0];consume=cs[1];
  VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};checked(fn<PFN_vkCreateFence>("vkCreateFence")(device,&fc,nullptr,&fence),"terminal fence");
  if(timestampBits){VkQueryPoolCreateInfo qc{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qc.queryType=VK_QUERY_TYPE_TIMESTAMP;qc.queryCount=20;
   checked(fn<PFN_vkCreateQueryPool>("vkCreateQueryPool")(device,&qc,nullptr,&timestamps),"timestamp pool");}
  r.raw("vulkan_timestamp_valid_bits",std::to_string(timestampBits));r.raw("vulkan_timestamp_period_ns",std::to_string(timestampPeriod));
 }
 void createShared(size_t index,const char* label,DXGI_FORMAT format,VkFormat vkFormat,uint32_t bpp) {
  stage("RESOURCE_CREATION");auto& im=images[index];im.label=label;im.dx.format=format;im.dx.bpp=bpp;im.format=vkFormat;
  D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};dx.check(dx.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof support),"format support");
  if(!(support.Support1&D3D12_FORMAT_SUPPORT1_TEXTURE2D)||!(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)) throw std::runtime_error("Unsupported historical format");
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;
  D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=W;desc.Height=H;desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=format;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  dx.check(dx.device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_SHARED,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&im.dx.resource)),"shared DEFAULT UAV texture");dx.owned.push_back(im.dx.resource);
  HANDLE handle{};dx.check(dx.device->CreateSharedHandle(im.dx.resource.Get(),nullptr,GENERIC_ALL,nullptr,&handle),"resource shared handle");unclosedHandles.push_back(handle);
  r.str(std::string(label)+"_d3d12_resource",handleText(im.dx.resource.Get()));r.str(std::string(label)+"_shared_handle",handleText(handle));
  stage("IMPORT"); constexpr auto usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  VkPhysicalDeviceExternalImageFormatInfo external{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};external.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
  VkPhysicalDeviceImageFormatInfo2 fi{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};fi.pNext=&external;fi.format=vkFormat;fi.type=VK_IMAGE_TYPE_2D;fi.tiling=VK_IMAGE_TILING_OPTIMAL;fi.usage=usage;
  VkExternalImageFormatProperties ep{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};VkImageFormatProperties2 fp{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};fp.pNext=&ep;
  auto result=fn<PFN_vkGetPhysicalDeviceImageFormatProperties2>("vkGetPhysicalDeviceImageFormatProperties2")(physical,&fi,&fp);
  r.raw(std::string(label)+"_format_query_result",std::to_string(result));checked(result,"exact external image format");
  if(!(ep.externalMemoryProperties.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)||fp.imageFormatProperties.maxExtent.width<W||fp.imageFormatProperties.maxExtent.height<H) throw std::runtime_error(std::string(label)+" exact format/usage/extent not importable");
  VkExternalMemoryImageCreateInfo ex{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};ex.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
  VkImageCreateInfo ic{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ic.pNext=&ex;ic.imageType=VK_IMAGE_TYPE_2D;ic.format=vkFormat;ic.extent={W,H,1};ic.mipLevels=ic.arrayLayers=1;ic.samples=VK_SAMPLE_COUNT_1_BIT;ic.tiling=VK_IMAGE_TILING_OPTIMAL;ic.usage=usage;ic.sharingMode=VK_SHARING_MODE_EXCLUSIVE;ic.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
  checked(fn<PFN_vkCreateImage>("vkCreateImage")(device,&ic,nullptr,&im.vk),"external image create");
  VkMemoryRequirements req{};fn<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(device,im.vk,&req);
  VkMemoryWin32HandlePropertiesKHR bits{VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR};checked(fn<PFN_vkGetMemoryWin32HandlePropertiesKHR>("vkGetMemoryWin32HandlePropertiesKHR")(device,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,handle,&bits),"handle memory bits");
  uint32_t type=intersectionType(req.memoryTypeBits,bits.memoryTypeBits,memoryProperties,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  r.str(std::string(label)+"_vk_image",handleText(im.vk));
  r.raw(std::string(label)+"_image_memory_type_bits",std::to_string(req.memoryTypeBits));r.raw(std::string(label)+"_handle_memory_type_bits",std::to_string(bits.memoryTypeBits));
  r.raw(std::string(label)+"_memory_type_bits",std::to_string(req.memoryTypeBits&bits.memoryTypeBits));r.raw(std::string(label)+"_memory_type_index",std::to_string(type));r.raw(std::string(label)+"_dedicated_allocation","true");
  VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.image=im.vk;
  VkImportMemoryWin32HandleInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};import.pNext=&dedicated;import.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;import.handle=handle;
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.pNext=&import;allocation.allocationSize=req.size;allocation.memoryTypeIndex=type;
  result=fn<PFN_vkAllocateMemory>("vkAllocateMemory")(device,&allocation,nullptr,&im.memory);r.raw(std::string(label)+"_import_result",std::to_string(result));checked(result,"dedicated resource import");
  r.str(std::string(label)+"_vk_memory",handleText(im.memory));
  CloseHandle(handle);unclosedHandles.back()=nullptr;
  handles<<(firstHandle?"":",")<<"{\"resource\":"<<jsonQuote(label)<<",\"handle\":"<<jsonQuote(handleText(handle))<<",\"import_consumes_handle\":false,\"application_closed_after_import\":true,\"payload_references\":[\"D3D12 resource\",\"VkDeviceMemory\"]}";firstHandle=false;
  stage("BIND");result=fn<PFN_vkBindImageMemory>("vkBindImageMemory")(device,im.vk,im.memory,0);r.raw(std::string(label)+"_bind_result",std::to_string(result));checked(result,"bind imported memory");
  r.str(std::string(label)+"_shared","PASS");r.str(std::string(label)+"_same_allocation","YES");
  resourceMap<<(firstResource?"":",")<<"{\"resource\":"<<jsonQuote(label)<<",\"d3d12_resource\":"<<jsonQuote(handleText(im.dx.resource.Get()))<<",\"vk_image\":"<<jsonQuote(handleText(im.vk))<<",\"vk_memory\":"<<jsonQuote(handleText(im.memory))<<",\"dxgi_format\":"<<format<<",\"vk_format\":"<<vkFormat<<",\"vk_usage\":"<<usage<<",\"external_memory_features\":"<<ep.externalMemoryProperties.externalMemoryFeatures<<",\"image_memory_type_bits\":"<<req.memoryTypeBits<<",\"handle_memory_type_bits\":"<<bits.memoryTypeBits<<",\"memory_type_bits\":"<<(req.memoryTypeBits&bits.memoryTypeBits)<<",\"memory_type_index\":"<<type<<",\"dedicated_allocation\":true,\"import_result\":0,\"bind_result\":0,\"vulkan_handoff_layout\":\"GENERAL\",\"d3d12_handoff_state\":\"COMMON\",\"d3d12_ngx_state\":"<<jsonQuote(index==4?"UNORDERED_ACCESS":"NON_PIXEL_SHADER_RESOURCE")<<"}";firstResource=false;
  std::ofstream(r.out/"resource-map.json")<<resourceMap.str()<<"]\n";
  std::ofstream(r.out/"handle-ownership.json")<<handles.str()<<"]\n";
 }
 void prepareSync() {
  stage("SYNC_VK_TO_D3D12");
  VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
  VkPhysicalDeviceExternalSemaphoreInfo si{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO};si.pNext=&type;si.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
  VkExternalSemaphoreProperties sp{VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};fn<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>("vkGetPhysicalDeviceExternalSemaphoreProperties")(physical,&si,&sp);
  r.raw("timeline_external_features",std::to_string(sp.externalSemaphoreFeatures));if(!(sp.externalSemaphoreFeatures&VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT))throw std::runtime_error("Timeline D3D12 fence not importable");
  dx.check(dx.device->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&sharedFence)),"shared fence create");
  HANDLE handle{};dx.check(dx.device->CreateSharedHandle(sharedFence.Get(),nullptr,GENERIC_ALL,nullptr,&handle),"fence shared handle");unclosedHandles.push_back(handle);
  VkSemaphoreCreateInfo sc{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};sc.pNext=&type;checked(fn<PFN_vkCreateSemaphore>("vkCreateSemaphore")(device,&sc,nullptr,&sharedTimeline),"timeline create");
  VkImportSemaphoreWin32HandleInfoKHR im{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR};im.semaphore=sharedTimeline;im.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;im.handle=handle;im.flags=0;
  checked(fn<PFN_vkImportSemaphoreWin32HandleKHR>("vkImportSemaphoreWin32HandleKHR")(device,&im),"permanent fence import");CloseHandle(handle);unclosedHandles.back()=nullptr;
  handles<<(firstHandle?"":",")<<"{\"resource\":\"timeline fence\",\"import_consumes_handle\":false,\"application_closed_after_import\":true,\"permanent_import\":true}";firstHandle=false;
  std::ofstream(r.out/"handle-ownership.json")<<handles.str()<<"]\n";
  r.str("sync_primitive","D3D12-owned shared fence; permanent Vulkan timeline import");
 }
 void start(VkCommandBuffer command) {
  checked(fn<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(command,0),"command reset");
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  checked(fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(command,&bi),"command begin");
 }
 void barrier(VkCommandBuffer command,SharedImage& im,bool acquire,bool release,VkAccessFlags dst) {
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=im.vk;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  b.oldLayout=im.initialized?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
  b.srcQueueFamilyIndex=acquire?VK_QUEUE_FAMILY_EXTERNAL:(release?queueIndex:VK_QUEUE_FAMILY_IGNORED);
  b.dstQueueFamilyIndex=release?VK_QUEUE_FAMILY_EXTERNAL:(acquire?queueIndex:VK_QUEUE_FAMILY_IGNORED);
  b.srcAccessMask=acquire||!im.initialized?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
  b.dstAccessMask=release?0:dst;
  fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(command,acquire||!im.initialized?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
   release?VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
  im.initialized=true;im.local=!release;
  r.event("vulkan_barrier",im.label+" old="+std::to_string(b.oldLayout)+" new=GENERAL srcFamily="+std::to_string(b.srcQueueFamilyIndex)+" dstFamily="+std::to_string(b.dstQueueFamilyIndex)+" srcAccess="+std::to_string(b.srcAccessMask)+" dstAccess="+std::to_string(b.dstAccessMask));
 }
 void copyOut(VkCommandBuffer command, SharedImage& im, Buffer dst) {
  barrier(command,im,false,false,VK_ACCESS_TRANSFER_READ_BIT); VkBufferImageCopy c{};c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};c.imageExtent={W,H,1};
  fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,im.vk,VK_IMAGE_LAYOUT_GENERAL,dst.b,1,&c);
  VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=dst.b;host.size=VK_WHOLE_SIZE;
  fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
 }
 void stamp(VkCommandBuffer command,uint32_t index,VkPipelineStageFlagBits stage) {
  if(timestamps)fn<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(command,stage,timestamps,index);
 }
 void submit(VkCommandBuffer command,uint64_t wait,uint64_t signal,bool terminal) {
  checked(fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(command),"command end");
  VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
  timeline.waitSemaphoreValueCount=wait?1:0;timeline.pWaitSemaphoreValues=&wait;timeline.signalSemaphoreValueCount=signal?1:0;timeline.pSignalSemaphoreValues=&signal;
  VkPipelineStageFlags stageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.pNext=&timeline;
  si.waitSemaphoreCount=wait?1:0;si.pWaitSemaphores=&sharedTimeline;si.pWaitDstStageMask=&stageMask;si.signalSemaphoreCount=signal?1:0;si.pSignalSemaphores=&sharedTimeline;
  si.commandBufferCount=1;si.pCommandBuffers=&command;
  if(terminal)checked(fn<PFN_vkResetFences>("vkResetFences")(device,1,&fence),"terminal fence reset");
  auto code=fn<PFN_vkQueueSubmit>("vkQueueSubmit")(queue,1,&si,terminal?fence:VK_NULL_HANDLE);checked(code,"cross API queue submit");pending=true;
  r.event("vulkan_submit","wait="+std::to_string(wait)+" signal="+std::to_string(signal)+" terminal="+(terminal?"true":"false"));
 }
 void terminalWait() {
  auto code=fn<PFN_vkWaitForFences>("vkWaitForFences")(device,1,&fence,VK_TRUE,15000000000ull);
  if(code!=VK_SUCCESS)unsafe=true;checked(code,"Vulkan consumer terminal completion");pending=false;
  auto reason=dx.device->GetDeviceRemovedReason();r.str("device_removed_reason",hrHex(reason));dx.removed=FAILED(reason);r.raw("device_removed",dx.removed?"true":"false");
  if(dx.removed){unsafe=true;stage("DEVICE_LOST");throw std::runtime_error("D3D12 device removed");}
  r.event("terminal_completion","CPU waits only AFTER Vulkan consumer; allocator reuse/validation, not cross-API handoff ordering");
 }
 Bytes finalRead(Buffer buffer,const std::string& label,bool rgba) {
  auto data=buffers.read(buffer);save(r.out/(label+".bin"),data);r.str(label+"_hash",sha(data));if(rgba)ppm(r.out/(label+".ppm"),data);return data;
 }
 void finish() {
  for(auto handle:unclosedHandles)if(handle)CloseHandle(handle);
  if(pending||unsafe||deviceLost||dx.removed){dx.unsafe=true;sharedFence.Detach();for(auto& im:images)im.dx.resource.Detach();r.event("cleanup","Unsafe or pending: no further GPU calls, process exit retains payload lifetimes");return;}
  buffers.close();for(auto& im:images){if(im.vk)fn<PFN_vkDestroyImage>("vkDestroyImage")(device,im.vk,nullptr);if(im.memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,im.memory,nullptr);}
  if(sharedTimeline)fn<PFN_vkDestroySemaphore>("vkDestroySemaphore")(device,sharedTimeline,nullptr);
  // Base close does not call NGX: initialized, parameters and feature stay false/null.
  VulkanNgxSession::close();
 }
};

static int combinedWorker(fs::path out,fs::path dll,fs::path runtime) {
 fs::create_directories(out);vendorLog.open(out/"ngx-callback.log");validationLog.open(out/"d3d12-debug.log");vkLog.open(out/"vulkan-validation.log");
 EvidenceRecorder r(out);auto start=std::chrono::steady_clock::now();
 r.str("api","VULKAN_D3D12_VULKAN");r.raw("generated_count_confirmed","0");r.raw("bridge_transport_cpu_copy_count","0");r.raw("validation_cpu_readback_count","0");
 r.raw("device_lost","false");r.raw("device_removed","false");r.raw("capabilities_are_generation_proof","false");r.str("cpu_wait_between_apis","NO");
 r.str("capability_provenance","REPORTED_POTENTIALLY_HOOKED");r.str("cause_confidence","UNKNOWN");r.str("end_to_end_worker","NOT_RUN");
 for(auto k:{"ngx_init","d3d12_createfeature","d3d12_evaluate","d3d12_warmup","gpu_completion","vulkan_input_write","vulkan_to_d3d12_gpu_sync","d3d12_to_vulkan_gpu_sync","vulkan_g1_readback","d3d12_output"})r.str(k,"NOT_RUN");
 ExternalLoader loader;D3D12NgxSession dx(r,runtime);SharedBridge vk(r,dx);int exit=1;
 try {
  if(sha(readFile(runtime/L"nvngx_dlssg.dll"))!="ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82")throw std::runtime_error("Historical runtime hash required");
  loader.start(r,dll);dx.initialize();vk.initializeInterop();
  if(r.checkpointFailed)throw std::runtime_error("Evidence persistence failed before shared resources");
  vk.createShared(0,"A",DXGI_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_UNORM,4);
  vk.createShared(1,"B",DXGI_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_UNORM,4);
  vk.createShared(2,"DEPTH",DXGI_FORMAT_R32_FLOAT,VK_FORMAT_R32_SFLOAT,4);
  vk.createShared(3,"MV",DXGI_FORMAT_R32G32_FLOAT,VK_FORMAT_R32G32_SFLOAT,8);
  vk.createShared(4,"G1",DXGI_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_UNORM,4);
  r.str("same_allocation_shared","YES");vk.prepareSync();
  std::ofstream(out/"fixture.json")<<"{\"width\":1280,\"height\":720,\"A_frame\":3,\"B_frame\":4,\"warmup_frames\":4,\"translation_pixels\":16,\"mvec_convention\":\"current_to_previous_NDC\",\"camera\":\"historical D3D12 PASS positive-Y projection\",\"jitter\":[0,0],\"count\":1,\"index\":1,\"fixture_source\":\"CPU generated -> Vulkan upload (allowed fixture initialization); shared allocation written by Vulkan GPU\",\"transport_CPU_copies\":0}\n";
  if(r.checkpointFailed){vk.stage("EVIDENCE_PERSISTENCE");throw std::runtime_error("Evidence persistence failed before CreateFeature");}
  vk.stage("FEATURE_CREATION");auto code=NVSDK_NGX_D3D12_AllocateParameters(&dx.parameters);r.str("allocate_parameters_result",resultHex(code));if(!NVSDK_NGX_SUCCEED(code)||!dx.parameters)throw std::runtime_error("AllocateParameters failed");
  NVSDK_NGX_DLSSG_Create_Params cp{};cp.Width=cp.RenderWidth=W;cp.Height=cp.RenderHeight=H;cp.NativeBackbufferFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
  NVSDK_NGX_Parameter_SetUI(dx.parameters,NVSDK_NGX_DLSSG_Parameter_Width,W);NVSDK_NGX_Parameter_SetUI(dx.parameters,NVSDK_NGX_DLSSG_Parameter_Height,H);
  dx.begin();code=NGX_D3D12_CREATE_DLSSG(dx.commands.Get(),1,1,&dx.feature,dx.parameters,&cp);
  r.str("create_result",resultHex(code));r.str("feature_handle",handleText(dx.feature));r.str("d3d12_createfeature",NVSDK_NGX_SUCCEED(code)&&dx.feature?"API_SUCCESS_PENDING_COMPLETION":"FAIL");r.modules("modules_after_create");
  if(!NVSDK_NGX_SUCCEED(code)||!dx.feature)throw std::runtime_error("CreateFeature failed "+resultHex(code));dx.complete("CreateFeature");r.str("d3d12_createfeature","PASS");
  auto disable=dx.buffer(16,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  auto disableUpload=dx.buffer(16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* mapped{};D3D12_RANGE empty{0,0};dx.check(disableUpload->Map(0,&empty,&mapped),"disable metadata upload");memset(mapped,255,16);disableUpload->Unmap(0,nullptr);
  ComPtr<ID3D12QueryHeap> dxQueries;D3D12_QUERY_HEAP_DESC qh{};qh.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;qh.Count=10;
  dx.check(dx.device->CreateQueryHeap(&qh,IID_PPV_ARGS(&dxQueries)),"timestamp heap");UINT64 dxFrequency{};dx.check(dx.queue->GetTimestampFrequency(&dxFrequency),"timestamp frequency");
  auto dxTicks=dx.buffer(80,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
  std::array<Buffer,5> upload{},readback{};
  for(size_t i=0;i<5;++i){auto bytes=size_t(W)*H*vk.images[i].dx.bpp;upload[i]=vk.buffers.buffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);readback[i]=vk.buffers.buffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);}
  auto sentinelReadback=vk.buffers.buffer(size_t(W)*H*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  std::ofstream sync(out/"sync-map.json");sync<<'[';
  for(int f=0;f<=4;++f) {
   if(r.checkpointFailed){vk.stage("EVIDENCE_PERSISTENCE");throw std::runtime_error("Evidence persistence failed before interval submit");}
   bool target=f==4;uint64_t ready=2*f+1,done=ready+1;vk.stage(target?"VULKAN_INPUT_WRITE":"WARMUP");
   std::array<Bytes,5> source{SyntheticFixture::color(float(std::min(f,3))),SyntheticFixture::color(4),SyntheticFixture::depths(f),SyntheticFixture::motion(f),SyntheticFixture::sentinel()};
   for(size_t i=0;i<5;++i)vk.buffers.write(upload[i],source[i]);
   vk.start(vk.input);if(f==0&&vk.timestamps)vk.fn<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(vk.input,vk.timestamps,0,20);
   vk.stamp(vk.input,2*f,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
   for(size_t i=0;i<5;++i){auto& im=vk.images[i];vk.barrier(vk.input,im,!im.local,false,VK_ACCESS_TRANSFER_WRITE_BIT);VkBufferImageCopy c{};c.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};c.imageExtent={W,H,1};vk.fn<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(vk.input,upload[i].b,im.vk,VK_IMAGE_LAYOUT_GENERAL,1,&c);}
   if(target)vk.copyOut(vk.input,vk.images[4],sentinelReadback);
   for(auto& im:vk.images)vk.barrier(vk.input,im,false,true,0);
   vk.stamp(vk.input,2*f+1,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);vk.submit(vk.input,0,ready,false);
   vk.stage("SYNC_VK_TO_D3D12");dx.check(dx.queue->Wait(vk.sharedFence.Get(),ready),"D3D12 wait Vulkan input-ready");
   r.event("d3d12_wait","ready="+std::to_string(ready));
   if(target){r.raw("vulkan_input_ready_signal_value",std::to_string(ready));r.raw("d3d12_input_ready_wait_value",std::to_string(ready));}
   dx.begin();auto& real=vk.images[target?1:0];vk.stage("RESOURCE_STATE");
   for(size_t i=0;i<5;++i){auto before=vk.images[i].dx.state;auto after=i==4?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;dx.barrier(vk.images[i].dx,after);r.event("d3d12_barrier",vk.images[i].label+" "+std::to_string(before)+" -> "+std::to_string(after));}
   dx.transition(disable.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);dx.commands->CopyBufferRegion(disable.Get(),0,disableUpload.Get(),0,16);dx.transition(disable.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   vk.stage(target?"EVALUATE":"WARMUP");NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};
   ep.pBackbuffer=ep.pHudless=real.dx.resource.Get();ep.pDepth=vk.images[2].dx.resource.Get();ep.pMVecs=vk.images[3].dx.resource.Get();ep.pOutputInterpFrame=vk.images[4].dx.resource.Get();ep.pOutputDisableInterpolation=disable.Get();
   auto opts=SyntheticFixture::options(f==0);opts.cameraViewToClip[1][1]*=-1;opts.clipToCameraView[1][1]*=-1;
   r.event("evaluate_begin","realFrameId="+std::to_string(f)+" count=1 index=1 reset="+(f==0?"true":"false")+" timestamp_ms="+std::to_string(f*16.666667)+" deltaMs=16.666667 outputSlot=0 D3D12="+handleText(ep.pOutputInterpFrame)+" VkImage="+handleText(vk.images[4].vk));
   dx.commands->EndQuery(dxQueries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2*f);
   code=NGX_D3D12_EVALUATE_DLSSG(dx.commands.Get(),dx.feature,dx.parameters,&ep,&opts);
   dx.commands->EndQuery(dxQueries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2*f+1);dx.commands->ResolveQueryData(dxQueries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2*f,2,dxTicks.Get(),16*f);
   r.str("evaluate_result",resultHex(code));r.event("evaluate_result",resultHex(code));
   if(!NVSDK_NGX_SUCCEED(code)){r.str("d3d12_evaluate","FAIL");throw std::runtime_error("Evaluate failed "+resultHex(code));}
   r.str("d3d12_evaluate",target?"API_SUCCESS_PENDING_COMPLETION":"WARMUP_SUCCESS");
   dx.barrier(vk.images[4].dx,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   for(auto& im:vk.images){auto before=im.dx.state;dx.barrier(im.dx,D3D12_RESOURCE_STATE_COMMON);r.event("d3d12_barrier",im.label+" "+std::to_string(before)+" -> COMMON");}
   vk.stage("D3D12_GPU_COMPLETION");dx.check(dx.commands->Close(),"Evaluate command close");ID3D12CommandList* lists[]={dx.commands.Get()};dx.queue->ExecuteCommandLists(1,lists);dx.check(dx.queue->Signal(vk.sharedFence.Get(),done),"shared DLSSG completion signal");
   r.event("d3d12_submit","realFrameId="+std::to_string(f)+" completeSignal="+std::to_string(done));
   if(target){r.raw("target_submit_result","0");r.str("d3d12_gpu_submit","PASS");r.raw("d3d12_dlssg_complete_signal_value",std::to_string(done));r.raw("vulkan_dlssg_complete_wait_value",std::to_string(done));}
   vk.stage("SYNC_D3D12_TO_VK");vk.start(vk.consume);vk.stamp(vk.consume,10+2*f,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
   for(auto& im:vk.images)vk.barrier(vk.consume,im,true,false,VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT);
   if(target)for(size_t i=0;i<5;++i)vk.copyOut(vk.consume,vk.images[i],readback[i]);
   vk.stamp(vk.consume,11+2*f,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);vk.submit(vk.consume,done,0,true);
   // No CPU wait happened between producer submit and consumer submit, in either direction.
   vk.terminalWait();r.event("interval_complete","realFrameId="+std::to_string(f)+" sharedFence="+std::to_string(vk.sharedFence->GetCompletedValue()));
   sync<<(f?",":"")<<"{\"realFrameId\":"<<f<<",\"count\":1,\"index\":1,\"Vulkan_input_ready_signal\":"<<ready<<",\"D3D12_input_ready_wait\":"<<ready<<",\"D3D12_DLSSG_complete_signal\":"<<done<<",\"Vulkan_DLSSG_complete_wait\":"<<done<<",\"CPU_ordering_between_APIs\":false,\"terminal_consumer_wait_for_reuse\":true}";sync.flush();
   if(!target)r.str("d3d12_warmup",f==3?"PASS":"IN_PROGRESS");
   else{r.raw("target_completion_result","0");r.str("gpu_completion","PASS");r.str("d3d12_evaluate","PASS");r.str("vulkan_to_d3d12_gpu_sync","PASS");r.str("d3d12_to_vulkan_gpu_sync","PASS");}
  }
  sync<<"]\n";sync.close();
  vk.stage("G1_READBACK");auto a=vk.finalRead(readback[0],"A",true),b=vk.finalRead(readback[1],"B",true),d=vk.finalRead(readback[2],"depth",false),m=vk.finalRead(readback[3],"motion",false),g=vk.finalRead(readback[4],"G1",true),sentinel=vk.finalRead(sentinelReadback,"sentinel",true);
  r.raw("validation_cpu_readback_count","6");r.str("vulkan_g1_readback","PASS");
  bool fixture=a==SyntheticFixture::color(3)&&b==SyntheticFixture::color(4)&&d==SyntheticFixture::depths(4)&&m==SyntheticFixture::motion(4)&&sentinel==SyntheticFixture::sentinel();r.raw("fixture_readback_valid",fixture?"true":"false");r.raw("real_inputs_preserved",fixture?"true":"false");r.str("vulkan_input_write",fixture?"PASS":"FAIL");
  if(!fixture){vk.stage("HASH_MISMATCH");throw std::runtime_error("Final shared input/sentinel hashes do not match historical fixture");}
  r.raw("output_disable_interpolation",std::to_string(dx.readDisable(disable.Get())));r.raw("validation_cpu_readback_count","7");
  D3D12_RANGE range{0,80};dx.check(dxTicks->Map(0,&range,&mapped),"DX timestamp readback");uint64_t ticks[10];memcpy(ticks,mapped,sizeof ticks);dxTicks->Unmap(0,&empty);
  r.raw("d3d12_timestamp_frequency_hz",std::to_string(dxFrequency));r.raw("dlssg_gpu_time_ms",std::to_string(double(ticks[9]-ticks[8])*1000.0/dxFrequency));
  r.str("dlssg_gpu_time_scope","D3D12 target Evaluate only; bottom-of-pipe timestamps, local queue frequency");
  if(vk.timestamps){uint64_t vkTicks[20];auto result=vk.fn<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(vk.device,vk.timestamps,0,20,sizeof vkTicks,vkTicks,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT);r.raw("vk_timestamp_query_result",std::to_string(result));if(result==VK_ERROR_DEVICE_LOST)vk.checked(result,"timestamp query");if(result==VK_SUCCESS){uint64_t mask=vk.timestampBits==64?UINT64_MAX:((uint64_t(1)<<vk.timestampBits)-1);r.raw("vulkan_input_gpu_time_ms",std::to_string(double((vkTicks[9]-vkTicks[8])&mask)*vk.timestampPeriod/1e6));r.raw("vulkan_g1_readback_gpu_time_ms",std::to_string(double((vkTicks[19]-vkTicks[18])&mask)*vk.timestampPeriod/1e6));}}
  r.raw("vulkan_to_d3d12_handoff_time_ms","null");r.raw("d3d12_to_vulkan_handoff_time_ms","null");r.str("timing_method","GPU timestamp scopes per API; no calibrated common clock, handoff times UNKNOWN; Vulkan consumer scope copies all five resources; total CPU wallclock includes initialization/warmup/logging/validation");
  r.raw("validation_cpu_readback_count","9");
  vk.stage("TEMPORAL_VALIDATION");bool valid=VerdictReducer::output(a,b,g,sentinel,r)&&g!=a&&g!=b&&g!=sentinel&&r.data["output_disable_interpolation"]=="0";
  r.str("d3d12_output",valid?"PASS":"FAIL");if(!valid)throw std::runtime_error("Vulkan G1 failed existing temporal reducer");
  r.str("end_to_end_worker","PASS_PENDING_KERNEL_AND_FINAL_EVIDENCE_REVIEW");r.str("if_fail_stage","NONE");r.str("cause_confidence","HIGH");exit=0;
 }catch(const std::exception& e){r.str("error",e.what());r.event("failure",e.what());r.str("end_to_end_worker","FAIL");r.str("cause_confidence","HIGH_STAGE_ONLY; root cause requires preserved public/backend evidence");}
 if(dx.device){auto reason=dx.device->GetDeviceRemovedReason();dx.removed=FAILED(reason);r.raw("device_removed",dx.removed?"true":"false");r.str("device_removed_reason",hrHex(reason));}
 if(vk.deviceLost||dx.removed)r.raw("device_lost","true");
 if(!dx.removed&&!dx.unsafe)dx.debugMessages();
 r.raw("d3d12_debug_errors",std::to_string(validationErrors));r.raw("vulkan_validation_errors",std::to_string(vkErrors));
 if(validationErrors||vkErrors){r.str("if_fail_stage","VALIDATION");r.str("end_to_end_worker","FAIL");exit=1;}
 try{vk.finish();dx.close();}catch(const std::exception& e){r.event("cleanup_failure",e.what());r.str("if_fail_stage","VALIDATION");r.str("end_to_end_worker","FAIL");exit=1;}
 r.raw("total_test_wallclock_ms",std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()));r.modules("modules_final");r.flush();return exit;
}

int wmain(int argc,wchar_t** argv) {
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc==3&&std::wstring(argv[1])==L"--self-test") {
  VkPhysicalDeviceMemoryProperties m{};m.memoryTypeCount=3;m.memoryTypes[2].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  if(intersectionType(6,4,m,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)!=2)return 1;
  bool rejected=false;try{intersectionType(2,4,m,0);}catch(...){rejected=true;}if(!rejected)return 1;
  return selfTest(fs::absolute(argv[2]));
 }
 if(argc!=4)return 2;
 return combinedWorker(fs::absolute(argv[1]),fs::absolute(argv[2]),fs::absolute(argv[3]));
}
