// Query-only NVIDIA preflight. Does not initialize NGX, D3D12 or Minecraft.
#define wmain UncalledCapabilityProbe
#include "probe.cpp"
#undef wmain
int wmain(int argc,wchar_t** argv) {
 if(argc!=2)return 2;auto out=fs::absolute(argv[1]);fs::create_directories(out);
 events.open(out/"format-query-events.jsonl");validation.open(out/"vulkan-validation.log");
 VkInstance instance{};PFN_vkGetInstanceProcAddr g{};int result=1;
 try {
  auto library=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  if(!library)throw std::runtime_error("System Vulkan loader unavailable");
  g=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library,"vkGetInstanceProcAddr"));
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Second-submit format preflight";app.apiVersion=VK_API_VERSION_1_2;
  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
  vk(load<PFN_vkCreateInstance>(g,{},"vkCreateInstance")(&ci,nullptr,&instance),"CreateInstance");
  uint32_t n{};auto en=load<PFN_vkEnumeratePhysicalDevices>(g,instance,"vkEnumeratePhysicalDevices");
  vk(en(instance,&n,nullptr),"physical count");std::vector<VkPhysicalDevice> devices(n);vk(en(instance,&n,devices.data()),"physical devices");
  VkPhysicalDevice physical{};VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&ids;
  for(auto p:devices){load<PFN_vkGetPhysicalDeviceProperties2>(g,instance,"vkGetPhysicalDeviceProperties2")(p,&props);
   if(props.properties.vendorID==0x10de&&hex(ids.deviceUUID,16)=="01895b66d1ca454d88788dd21fdef638"){physical=p;break;}}
  if(!physical)throw std::runtime_error("Previously identified NVIDIA UUID absent");
  put("gpu",props.properties.deviceName);put("uuid",hex(ids.deviceUUID,16));put("luid",hex(ids.deviceLUID,8));
  auto dex=load<PFN_vkEnumerateDeviceExtensionProperties>(g,instance,"vkEnumerateDeviceExtensionProperties");vk(dex(physical,nullptr,&n,nullptr),"extension count");std::vector<VkExtensionProperties> extensions(n);vk(dex(physical,nullptr,&n,extensions.data()),"extensions");
  for(auto name:{"VK_EXT_device_fault","VK_NV_device_diagnostic_checkpoints","VK_KHR_synchronization2"})raw(name,std::any_of(extensions.begin(),extensions.end(),[&](auto&e){return std::string(e.extensionName)==name;})?"true":"false");
  auto layers=load<PFN_vkEnumerateInstanceLayerProperties>(g,{},"vkEnumerateInstanceLayerProperties");vk(layers(&n,nullptr),"layer count");std::vector<VkLayerProperties> ls(n);vk(layers(&n,ls.data()),"layers");
  raw("validation_available",std::any_of(ls.begin(),ls.end(),[](auto&l){return std::string(l.layerName)=="VK_LAYER_KHRONOS_validation";})?"true":"false");raw("validation_enabled","false");
  bool safe=true;
  for(auto pair:std::vector<std::pair<const char*,VkFormat>>{{"R32F",VK_FORMAT_R32_SFLOAT},{"RG16F",VK_FORMAT_R16G16_SFLOAT},{"RG32F",VK_FORMAT_R32G32_SFLOAT}}){
   auto prefix=std::string(pair.first);VkFormatProperties fp{};load<PFN_vkGetPhysicalDeviceFormatProperties>(g,instance,"vkGetPhysicalDeviceFormatProperties")(physical,pair.second,&fp);
   raw(prefix+"_optimal_features",std::to_string(fp.optimalTilingFeatures));raw(prefix+"_BLIT_SRC",(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_SRC_BIT)?"true":"false");raw(prefix+"_BLIT_DST",(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT)?"true":"false");
   VkPhysicalDeviceImageFormatInfo2 fi{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};fi.format=pair.second;fi.type=VK_IMAGE_TYPE_2D;fi.tiling=VK_IMAGE_TILING_OPTIMAL;fi.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   VkImageFormatProperties2 ip{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};auto r=load<PFN_vkGetPhysicalDeviceImageFormatProperties2>(g,instance,"vkGetPhysicalDeviceImageFormatProperties2")(physical,&fi,&ip);
   raw(prefix+"_image_query_result",std::to_string(r));raw(prefix+"_usage",std::to_string(fi.usage));raw(prefix+"_max_width",std::to_string(ip.imageFormatProperties.maxExtent.width));raw(prefix+"_max_height",std::to_string(ip.imageFormatProperties.maxExtent.height));raw(prefix+"_samples",std::to_string(ip.imageFormatProperties.sampleCounts));
   safe &= r==VK_SUCCESS&&(ip.imageFormatProperties.sampleCounts&VK_SAMPLE_COUNT_1_BIT)&&ip.imageFormatProperties.maxExtent.width>=495&&ip.imageFormatProperties.maxExtent.height>=278;
   safe &= prefix=="RG32F"?bool(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT):bool(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_SRC_BIT);
   if(prefix=="R32F")safe &= bool(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT);
  }
  put("filter","NEAREST");raw("linear_filter_required","false");put("motion_conversion","RG16F_TO_RG32F_FLOAT_COLOR_BLIT");put("format_gate",safe?"PASS":"FAIL");raw("gpu_blit_executed","false");raw("minecraft_launched","false");raw("ngx_initialized","false");
  if(!safe)throw std::runtime_error("Format/usage/extent/sample gate failed");result=0;
 } catch(const std::exception& e){put("error",e.what());put("format_gate","FAIL");}
 if(instance)load<PFN_vkDestroyInstance>(g,instance,"vkDestroyInstance")(instance,nullptr);write(out/"format-preflight.json");return result;
}
