// Experimental local bridge. Only the already-audited public loader and NGX helpers
// are reused. No private community export is resolved or invoked.
#define wmain UncalledHistoricalHarnessEntry
#include "../../../experiments/dlssg-external-harness/harness.cpp"
#undef wmain
#include <jni.h>
#include <array>
#include <memory>

namespace integration {
static void hr(HRESULT h, const char* what) {
    if (FAILED(h)) throw std::runtime_error(std::string(what)+" "+hrHex(h));
}
static void vkcheck(VkResult v, const char* what) {
    if (v != VK_SUCCESS) throw std::runtime_error(std::string(what)+" VkResult="+std::to_string(v));
}
struct Image {
    D3DImage dx;
    VkImage image{}; VkDeviceMemory memory{}; VkImageView view{};
    VkFormat format{}; uint32_t width{}, height{};
    bool initialized=false;
};
struct Readback {
    ComPtr<ID3D12Resource> buffer;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 size{};
};
struct VkSample {
    VkBuffer buffer{}; VkDeviceMemory memory{}; size_t size{}; bool coherent=false;
};
struct Slot {
    // color, HUDless, depth, motion, G1. All handles are imported once.
    std::array<Image,5> images;
    VkCommandBuffer inputs{};
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Resource> disable, disableReadback, timingReadback;
    ComPtr<ID3D12QueryHeap> timing;
    VkSample colorSample, generatedSample;
    uint64_t ready{}, done{}, frameId{};
    bool leased=false, prepared=false, submitted=false, sampled=false, reset=false;
    std::array<bool,5> priorInitialized{};
    uint32_t timestampIndex{};
};
struct Pool {
    unsigned width{},height{},renderWidth{},renderHeight{};
    NVSDK_NGX_Parameter* parameters{}; NVSDK_NGX_Handle* feature{};
    std::vector<std::unique_ptr<Slot>> slots;
    bool failed=false;
    VkQueryPool inputTiming{};
};
struct Session {
    EvidenceRecorder evidence;
    ExternalLoader external;
    D3D12NgxSession dx;
    VkInstance instance{}; VkPhysicalDevice physical{}; VkDevice device{}; VkQueue queue{};
    uint32_t family{}; PFN_vkGetInstanceProcAddr gipa{}; PFN_vkGetDeviceProcAddr gdpa{};
    VkCommandPool commandPool{}; VkSemaphore timeline{}; VkQueryPool vkTiming{};
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12Resource> disableSentinel;
    uint64_t sequence{}, dxFrequency{}, submittedFrames{}, completedFrames{}, generatedFrames{};
    float vkPeriod{}; uint32_t vkTimestampBits{};
    bool unsafe=false; std::vector<std::unique_ptr<Pool>> pools;
    std::ofstream events;
    std::string previousSampleHash; uint64_t previousSampleId{};
    std::map<VkFormat,VkFormatProperties> formatProperties;

    template<class F> F fn(const char* name) {
        auto p=gdpa?gdpa(device,name):nullptr;
        if(!p) p=gipa(instance,name);
        if(!p) throw std::runtime_error(std::string("Missing Vulkan function ")+name);
        return reinterpret_cast<F>(p);
    }
    void event(const char* type, const std::string& detail) {
        events<<"{\"timestamp_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
              <<",\"type\":"<<jsonQuote(type)<<",\"detail\":"<<jsonQuote(detail)<<"}\n";
        // Stream buffering is intentional; no per-frame checkpoint replacement or sleep.
    }
    Session(fs::path out, fs::path dll, fs::path runtime):evidence(out),dx(evidence,runtime),events(out/"provider-events.jsonl") {
        if(sha(readFile(runtime/"nvngx_dlssg.dll"))!="ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82")
            throw std::runtime_error("Runtime SHA256 mismatch");
        vendorLog.open(out/"ngx.log"); validationLog.open(out/"d3d12-debug.log");
        external.start(evidence,dll); dx.initialize();
        hr(dx.queue->GetTimestampFrequency(&dxFrequency),"D3D12 timestamp frequency");
        disableSentinel=dx.buffer(16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* map{};D3D12_RANGE none{0,0};hr(disableSentinel->Map(0,&none,&map),"disable sentinel map");std::memset(map,255,16);disableSentinel->Unmap(0,nullptr);
    }
    void borrow(VkInstance i, VkPhysicalDevice p, VkDevice d, VkQueue q, uint32_t f, PFN_vkGetInstanceProcAddr proc) {
        instance=i;physical=p;device=d;queue=q;family=f;gipa=proc;
        gdpa=reinterpret_cast<PFN_vkGetDeviceProcAddr>(gipa(instance,"vkGetDeviceProcAddr"));
        VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&id;
        fn<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(physical,&props);
        if(hexBytes(id.deviceUUID,VK_UUID_SIZE)!="01895b66d1ca454d88788dd21fdef638"||!id.deviceLUIDValid||hexBytes(id.deviceLUID,VK_LUID_SIZE)!="4c29010000000000")
            throw std::runtime_error("Borrowed Vulkan GPU UUID/LUID mismatch");
        evidence.str("vulkan_gpu_uuid",hexBytes(id.deviceUUID,VK_UUID_SIZE));
        evidence.str("vulkan_gpu_luid",hexBytes(id.deviceLUID,VK_LUID_SIZE));
        const auto driver=std::to_string(props.properties.driverVersion>>22)+"."+std::to_string((props.properties.driverVersion>>14)&255);
        evidence.str("driver",driver);
        if(driver!="596.49")throw std::runtime_error("Driver differs from identified 596.49 baseline");
        vkPeriod=props.properties.limits.timestampPeriod;
        uint32_t n{};fn<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(physical,&n,nullptr);
        std::vector<VkQueueFamilyProperties> qs(n);fn<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(physical,&n,qs.data());
        if(family>=n) throw std::runtime_error("Invalid borrowed queue family");vkTimestampBits=qs[family].timestampValidBits;
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=family;pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkcheck(fn<PFN_vkCreateCommandPool>("vkCreateCommandPool")(device,&pc,nullptr,&commandPool),"input command pool");
        hr(dx.device->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&fence)),"shared fence");
        HANDLE handle{};hr(dx.device->CreateSharedHandle(fence.Get(),nullptr,GENERIC_ALL,nullptr,&handle),"fence handle");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkPhysicalDeviceExternalSemaphoreInfo semaphoreQuery{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO};semaphoreQuery.pNext=&type;semaphoreQuery.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
        VkExternalSemaphoreProperties semaphoreProperties{VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES};
        fn<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>("vkGetPhysicalDeviceExternalSemaphoreProperties")(physical,&semaphoreQuery,&semaphoreProperties);
        if(!(semaphoreProperties.externalSemaphoreFeatures&VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT)){CloseHandle(handle);throw std::runtime_error("D3D12 fence timeline not importable");}
        VkSemaphoreCreateInfo sc{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};sc.pNext=&type;
        auto code=fn<PFN_vkCreateSemaphore>("vkCreateSemaphore")(device,&sc,nullptr,&timeline);
        if(code!=VK_SUCCESS){CloseHandle(handle);vkcheck(code,"shared timeline create");}
        VkImportSemaphoreWin32HandleInfoKHR im{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR};im.semaphore=timeline;im.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;im.handle=handle;
        code=fn<PFN_vkImportSemaphoreWin32HandleKHR>("vkImportSemaphoreWin32HandleKHR")(device,&im);CloseHandle(handle);vkcheck(code,"permanent shared fence import");
        evidence.str("sync_primitive","D3D12 shared fence / permanently imported Vulkan timeline");
        evidence.raw("per_frame_cpu_wait_between_apis","false");
    }
    Image createImage(uint32_t w,uint32_t h,DXGI_FORMAT format,VkFormat vf) {
        Image im;im.width=w;im.height=h;im.format=vf;im.dx.format=format;
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};hr(dx.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof support),"format support");
        if(!(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)) throw std::runtime_error("D3D12 typed UAV store unsupported");
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;
        D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;rd.Width=w;rd.Height=h;rd.DepthOrArraySize=rd.MipLevels=1;rd.Format=format;rd.SampleDesc.Count=1;rd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        hr(dx.device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_SHARED,&rd,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&im.dx.resource)),"shared image create");
        const VkImageUsageFlags usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkPhysicalDeviceExternalImageFormatInfo ex{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};ex.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
        VkPhysicalDeviceImageFormatInfo2 fi{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};fi.pNext=&ex;fi.format=vf;fi.type=VK_IMAGE_TYPE_2D;fi.tiling=VK_IMAGE_TILING_OPTIMAL;fi.usage=usage;
        VkExternalImageFormatProperties ep{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};VkImageFormatProperties2 fp{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};fp.pNext=&ep;
        vkcheck(fn<PFN_vkGetPhysicalDeviceImageFormatProperties2>("vkGetPhysicalDeviceImageFormatProperties2")(physical,&fi,&fp),"exact shared format query");
        if(!(ep.externalMemoryProperties.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)||w>fp.imageFormatProperties.maxExtent.width||h>fp.imageFormatProperties.maxExtent.height)throw std::runtime_error("Shared format/extent not importable");
        VkExternalMemoryImageCreateInfo ei{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};ei.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
        VkImageCreateInfo ic{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ic.pNext=&ei;ic.imageType=VK_IMAGE_TYPE_2D;ic.extent={w,h,1};ic.mipLevels=ic.arrayLayers=1;ic.samples=VK_SAMPLE_COUNT_1_BIT;ic.tiling=VK_IMAGE_TILING_OPTIMAL;ic.usage=usage;ic.format=vf;
        vkcheck(fn<PFN_vkCreateImage>("vkCreateImage")(device,&ic,nullptr,&im.image),"import image create");
        HANDLE handle{};hr(dx.device->CreateSharedHandle(im.dx.resource.Get(),nullptr,GENERIC_ALL,nullptr,&handle),"image handle");
        VkMemoryRequirements req{};fn<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(device,im.image,&req);
        VkMemoryWin32HandlePropertiesKHR bits{VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR};
        auto code=fn<PFN_vkGetMemoryWin32HandlePropertiesKHR>("vkGetMemoryWin32HandlePropertiesKHR")(device,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,handle,&bits);
        if(code!=VK_SUCCESS){CloseHandle(handle);vkcheck(code,"handle memory properties");}
        VkPhysicalDeviceMemoryProperties mp{};fn<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical,&mp);
        uint32_t type=UINT32_MAX;for(uint32_t j=0;j<mp.memoryTypeCount;++j)if((req.memoryTypeBits&bits.memoryTypeBits&(1u<<j))&&(mp.memoryTypes[j].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=j;break;}
        if(type==UINT32_MAX){CloseHandle(handle);throw std::runtime_error("No image/handle DEVICE_LOCAL intersection");}
        VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.image=im.image;
        VkImportMemoryWin32HandleInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};import.pNext=&dedicated;import.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;import.handle=handle;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.pNext=&import;ai.allocationSize=req.size;ai.memoryTypeIndex=type;
        code=fn<PFN_vkAllocateMemory>("vkAllocateMemory")(device,&ai,nullptr,&im.memory);CloseHandle(handle);vkcheck(code,"dedicated shared memory import");
        vkcheck(fn<PFN_vkBindImageMemory>("vkBindImageMemory")(device,im.image,im.memory,0),"shared image bind");
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=im.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=vf;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkcheck(fn<PFN_vkCreateImageView>("vkCreateImageView")(device,&vi,nullptr,&im.view),"shared view create");
        event("resource_pool", "image="+handleText(im.image)+" memory="+handleText(im.memory)+" D3D12="+handleText(im.dx.resource.Get())+" format="+std::to_string(vf)+" extent="+std::to_string(w)+"x"+std::to_string(h)+" memoryType="+std::to_string(type)+" handle_closed_after_import=true");
        return im;
    }
    Readback createReadback(Image& image) {
        Readback rb;auto rd=image.dx.resource->GetDesc();UINT rows{};UINT64 bytes{};
        dx.device->GetCopyableFootprints(&rd,0,1,0,&rb.footprint,&rows,&bytes,&rb.size);
        rb.buffer=dx.buffer(rb.size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);return rb;
    }
    VkSample createSample(unsigned w,unsigned h) {
        VkSample rb;rb.size=size_t(w)*h*4;VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=rb.size;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vkcheck(fn<PFN_vkCreateBuffer>("vkCreateBuffer")(device,&bi,nullptr,&rb.buffer),"diagnostic buffer create");
        VkMemoryRequirements req{};fn<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(device,rb.buffer,&req);
        VkPhysicalDeviceMemoryProperties props{};fn<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical,&props);
        unsigned type=UINT32_MAX;for(unsigned j=0;j<props.memoryTypeCount;++j)if((req.memoryTypeBits&(1u<<j))&&(props.memoryTypes[j].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){type=j;if(props.memoryTypes[j].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)break;}
        if(type==UINT32_MAX)throw std::runtime_error("No diagnostic host-visible buffer memory");rb.coherent=(props.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=type;
        vkcheck(fn<PFN_vkAllocateMemory>("vkAllocateMemory")(device,&ai,nullptr,&rb.memory),"diagnostic memory create");vkcheck(fn<PFN_vkBindBufferMemory>("vkBindBufferMemory")(device,rb.buffer,rb.memory,0),"diagnostic buffer bind");return rb;
    }
    void sampleCopy(VkCommandBuffer command,Image& im,VkSample& rb) {
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={im.width,im.height,1};
        fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,im.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,rb.buffer,1,&copy);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=rb.buffer;host.size=VK_WHOLE_SIZE;host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    }
    Bytes pixels(VkSample& sample) {
        void* mapped{};vkcheck(fn<PFN_vkMapMemory>("vkMapMemory")(device,sample.memory,0,VK_WHOLE_SIZE,0,&mapped),"completed Vulkan sample map");
        if(!sample.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=sample.memory;range.size=VK_WHOLE_SIZE;vkcheck(fn<PFN_vkInvalidateMappedMemoryRanges>("vkInvalidateMappedMemoryRanges")(device,1,&range),"sample invalidate");}
        Bytes out(sample.size);std::memcpy(out.data(),mapped,out.size());fn<PFN_vkUnmapMemory>("vkUnmapMemory")(device,sample.memory);return out;
    }
    Pool* createPool(unsigned w,unsigned h,unsigned rw,unsigned rh,unsigned capacity) {
        if(!w||!h||!rw||!rh||capacity<2||capacity>64)throw std::runtime_error("Invalid pool contract");
        auto holder=std::make_unique<Pool>();auto p=holder.get();pools.push_back(std::move(holder));
        p->width=w;p->height=h;p->renderWidth=rw;p->renderHeight=rh;
        if(vkTimestampBits){VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=capacity*2;vkcheck(fn<PFN_vkCreateQueryPool>("vkCreateQueryPool")(device,&qi,nullptr,&p->inputTiming),"input timing query pool");}
        for(unsigned i=0;i<capacity;++i){auto ptr=std::make_unique<Slot>();auto& s=*ptr;p->slots.push_back(std::move(ptr));
            for(int j=0;j<5;++j)s.images[j]=createImage(j==2||j==3?rw:w,j==2||j==3?rh:h,j==2?DXGI_FORMAT_R32_FLOAT:j==3?DXGI_FORMAT_R32G32_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM,j==2?VK_FORMAT_R32_SFLOAT:j==3?VK_FORMAT_R32G32_SFLOAT:VK_FORMAT_R8G8B8A8_UNORM);
            VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commandPool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
            vkcheck(fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(device,&ca,&s.inputs),"slot input command allocation");
            hr(dx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&s.allocator)),"slot allocator");
            hr(dx.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,s.allocator.Get(),nullptr,IID_PPV_ARGS(&s.commands)),"slot command list");hr(s.commands->Close(),"slot initial close");
            s.disable=dx.buffer(16,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            s.timestampIndex=i*2;
            s.disableReadback=dx.buffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_QUERY_HEAP_DESC q{};q.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;q.Count=2;hr(dx.device->CreateQueryHeap(&q,IID_PPV_ARGS(&s.timing)),"slot timestamp heap");
            s.timingReadback=dx.buffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            // Pixel readback objects exist only for the bounded diagnostic sampling window.
            if(i<3){s.colorSample=createSample(w,h);s.generatedSample=createSample(w,h);}
        }
        auto code=NVSDK_NGX_D3D12_AllocateParameters(&p->parameters);if(!NVSDK_NGX_SUCCEED(code)||!p->parameters)throw std::runtime_error("Allocate feature parameters "+resultHex(code));
        NVSDK_NGX_DLSSG_Create_Params cp{};cp.Width=w;cp.Height=h;cp.RenderWidth=rw;cp.RenderHeight=rh;cp.NativeBackbufferFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
        NVSDK_NGX_Parameter_SetUI(p->parameters,NVSDK_NGX_DLSSG_Parameter_Width,w);NVSDK_NGX_Parameter_SetUI(p->parameters,NVSDK_NGX_DLSSG_Parameter_Height,h);
        NVSDK_NGX_Parameter_SetUI(p->parameters,NVSDK_NGX_DLSSG_Parameter_UserInterfaceRecompositionEnabled,0);
        dx.begin();code=NGX_D3D12_CREATE_DLSSG(dx.commands.Get(),1,1,&p->feature,p->parameters,&cp);
        evidence.str("create_result",resultHex(code));if(!NVSDK_NGX_SUCCEED(code)||!p->feature)throw std::runtime_error("CreateFeature "+resultHex(code));
        // This bounded setup wait is outside the per-frame handoff path.
        dx.complete("Minecraft pool CreateFeature");evidence.str("minecraft_create_feature","PASS");
        return p;
    }
    void sharedBarrier(VkCommandBuffer command, Image& im, bool acquire, bool release, VkImageLayout newLayout=VK_IMAGE_LAYOUT_GENERAL) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=im.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        b.oldLayout=im.initialized?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=newLayout;
        b.srcQueueFamilyIndex=acquire?VK_QUEUE_FAMILY_EXTERNAL:release?family:VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex=release?VK_QUEUE_FAMILY_EXTERNAL:acquire?family:VK_QUEUE_FAMILY_IGNORED;
        b.srcAccessMask=acquire||!im.initialized?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=release?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(command,acquire||!im.initialized?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,release?VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
        im.initialized=true;
    }
    void sourceBarrier(VkCommandBuffer c,VkImage image,int before,int after) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.oldLayout=VkImageLayout(before);b.newLayout=VkImageLayout(after);b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(c,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
    }
    void dxBarrier(Slot& s,Image& im,D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={im.dx.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,im.dx.state,after};
        if(im.dx.state!=after)s.commands->ResourceBarrier(1,&b);im.dx.state=after;
    }
    void readbackCopy(Slot& s,Image& im,Readback& rb) {
        dxBarrier(s,im,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=im.dx.resource.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=rb.buffer.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=rb.footprint;
        s.commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);dxBarrier(s,im,D3D12_RESOURCE_STATE_COMMON);
    }
    void prepare(Pool& p,Slot& s,VkCommandBuffer output,const std::vector<jlong>& src,const std::vector<float>& constants,uint64_t frameId,double delta,bool reset,bool flipBorrowedInputs,bool sample) {
        if(s.leased||unsafe||p.failed||src.size()!=20||constants.size()!=108)throw std::runtime_error("Invalid or failed slot/metadata contract");
        s.leased=true;s.prepared=false;s.submitted=false;s.frameId=frameId;s.sampled=sample&&s.colorSample.buffer;s.reset=reset;
        for(size_t j=0;j<5;++j)s.priorInitialized[j]=s.images[j].initialized;
        s.ready=++sequence;s.done=++sequence;
        vkcheck(fn<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(s.inputs,0),"input reset");VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkcheck(fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(s.inputs,&bi),"input begin");
        if(p.inputTiming){fn<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(s.inputs,p.inputTiming,s.timestampIndex,2);fn<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(s.inputs,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,p.inputTiming,s.timestampIndex);}
        event("slot_state","realFrameId="+std::to_string(frameId)+" state=FILLING");
        for(int j=0;j<5;++j){auto& im=s.images[j];
            // Previous output acquire was TRANSFER_SRC for the two presented images.
            if(im.initialized&&(j==0||j==4))sourceBarrier(s.inputs,im.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL);
            sharedBarrier(s.inputs,im,false,false);
            if(j<4){auto image=reinterpret_cast<VkImage>(src[j*5]);auto width=uint32_t(src[j*5+1]);auto height=uint32_t(src[j*5+2]);auto format=VkFormat(src[j*5+3]);auto layout=int(src[j*5+4]);
                if(width!=im.width||height!=im.height)throw std::runtime_error("Input extent mismatch");
                for(auto f:{format,im.format})if(!formatProperties.count(f)){VkFormatProperties properties{};fn<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties")(physical,f,&properties);formatProperties[f]=properties;}
                const auto a=formatProperties.at(format),b=formatProperties.at(im.format);
                if(!(a.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_SRC_BIT)||!(b.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT))throw std::runtime_error("Input format conversion not supported by blit");
                sourceBarrier(s.inputs,image,layout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]={int(width),int(height),1};region.dstOffsets[1]={int(width),int(height),1};
                if(j>=2&&flipBorrowedInputs){region.srcOffsets[0].y=int(height);region.srcOffsets[1].y=0;}
                fn<PFN_vkCmdBlitImage>("vkCmdBlitImage")(s.inputs,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,im.image,VK_IMAGE_LAYOUT_GENERAL,1,&region,VK_FILTER_NEAREST);
                sourceBarrier(s.inputs,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,layout);
            }else{VkClearColorValue sentinel{};sentinel.float32[0]=sentinel.float32[2]=sentinel.float32[3]=1;VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};fn<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(s.inputs,im.image,VK_IMAGE_LAYOUT_GENERAL,&sentinel,1,&range);}
            sharedBarrier(s.inputs,im,false,true);
            // Recorded into the worker's already-begun output command buffer; its GPU
            // timeline wait is added by the provider submission plan after input submit.
            sharedBarrier(output,im,true,false,j==0||j==4?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL);
        }
        if(s.sampled){sampleCopy(output,s.images[0],s.colorSample);sampleCopy(output,s.images[4],s.generatedSample);}
        if(p.inputTiming)fn<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(s.inputs,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,p.inputTiming,s.timestampIndex+1);
        vkcheck(fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(s.inputs),"input end");
        hr(s.allocator->Reset(),"slot allocator reset");hr(s.commands->Reset(s.allocator.Get(),nullptr),"slot list reset");
        D3D12_RESOURCE_BARRIER flagBarrier{};flagBarrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;flagBarrier.Transition={s.disable.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST};s.commands->ResourceBarrier(1,&flagBarrier);s.commands->CopyBufferRegion(s.disable.Get(),0,disableSentinel.Get(),0,16);std::swap(flagBarrier.Transition.StateBefore,flagBarrier.Transition.StateAfter);s.commands->ResourceBarrier(1,&flagBarrier);
        for(int j=0;j<5;++j)dxBarrier(s,s.images[j],j==4?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};ep.pBackbuffer=s.images[0].dx.resource.Get();ep.pHudless=s.images[1].dx.resource.Get();ep.pDepth=s.images[2].dx.resource.Get();ep.pMVecs=s.images[3].dx.resource.Get();ep.pOutputInterpFrame=s.images[4].dx.resource.Get();ep.pOutputDisableInterpolation=s.disable.Get();
        NVSDK_NGX_DLSSG_Opt_Eval_Params opts{};opts.multiFrameCount=opts.multiFrameIndex=1;
        // All five matrices have already been converted to row-major DX clip space.
        std::memcpy(opts.cameraViewToClip,constants.data(),16*sizeof(float));
        std::memcpy(opts.clipToCameraView,constants.data()+16,16*sizeof(float));
        std::memcpy(opts.clipToLensClip,constants.data()+32,16*sizeof(float));
        std::memcpy(opts.clipToPrevClip,constants.data()+48,16*sizeof(float));
        std::memcpy(opts.prevClipToClip,constants.data()+64,16*sizeof(float));
        auto v=constants.data()+80;opts.jitterOffset[0]=v[0];opts.jitterOffset[1]=v[1];opts.mvecScale[0]=v[2];opts.mvecScale[1]=v[3];
        opts.cameraPinholeOffset[0]=v[4];opts.cameraPinholeOffset[1]=v[5];std::memcpy(opts.cameraPos,v+6,3*sizeof(float));std::memcpy(opts.cameraUp,v+9,3*sizeof(float));std::memcpy(opts.cameraRight,v+12,3*sizeof(float));std::memcpy(opts.cameraFwd,v+15,3*sizeof(float));opts.cameraNear=v[18];opts.cameraFar=v[19];opts.cameraFOV=v[20];opts.cameraAspectRatio=v[21];opts.depthInverted=v[22]!=0;opts.cameraMotionIncluded=v[23]!=0;opts.motionVectorsInvalidValue=v[24];opts.motionVectorsDilated=v[25]!=0;opts.orthoProjection=v[26]!=0;opts.minRelativeLinearDepthObjectSeparation=v[27];opts.reset=reset;
        opts.mvecsSubrectSize=opts.depthSubrectSize={p.renderWidth,p.renderHeight};opts.hudLessSubrectSize=opts.backbufferSubrectSize=opts.outputInterpSubrectSize={p.width,p.height};
        s.commands->EndQuery(s.timing.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
        NVSDK_NGX_Parameter_SetULL(p.parameters,NVSDK_NGX_DLSSG_Parameter_BackbufferFrameID,frameId);
        auto code=NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(),p.feature,p.parameters,&ep,&opts);
        event("evaluate", "realFrameId="+std::to_string(frameId)+" deltaMs="+std::to_string(delta)+" reset="+std::to_string(reset)+" count=1 index=1 result="+resultHex(code)+" VkImage="+handleText(s.images[4].image)+" ready="+std::to_string(s.ready)+" done="+std::to_string(s.done));
        if(!NVSDK_NGX_SUCCEED(code)){p.failed=true;throw std::runtime_error("NGX Evaluate "+resultHex(code));}
        s.commands->EndQuery(s.timing.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);s.commands->ResolveQueryData(s.timing.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,s.timingReadback.Get(),0);
        for(auto& im:s.images)dxBarrier(s,im,D3D12_RESOURCE_STATE_COMMON);
        D3D12_RESOURCE_BARRIER db{};db.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;db.Transition={s.disable.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};s.commands->ResourceBarrier(1,&db);s.commands->CopyBufferRegion(s.disableReadback.Get(),0,s.disable.Get(),0,16);std::swap(db.Transition.StateBefore,db.Transition.StateAfter);s.commands->ResourceBarrier(1,&db);
        hr(s.commands->Close(),"slot list close");s.prepared=true;
    }
    void submit(Slot& s,const std::vector<jlong>& semaphores) {
        if(!s.prepared||s.submitted||unsafe)throw std::runtime_error("Invalid input submission state");
        std::vector<VkSemaphore> waits;for(auto v:semaphores)waits.push_back(reinterpret_cast<VkSemaphore>(v));std::vector<VkPipelineStageFlags> stages(waits.size(),VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);std::vector<uint64_t> zeros(waits.size(),0);
        VkTimelineSemaphoreSubmitInfo ti{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};ti.waitSemaphoreValueCount=uint32_t(waits.size());ti.pWaitSemaphoreValues=zeros.data();ti.signalSemaphoreValueCount=1;ti.pSignalSemaphoreValues=&s.ready;
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.pNext=&ti;si.commandBufferCount=1;si.pCommandBuffers=&s.inputs;si.waitSemaphoreCount=uint32_t(waits.size());si.pWaitSemaphores=waits.data();si.pWaitDstStageMask=stages.data();si.signalSemaphoreCount=1;si.pSignalSemaphores=&timeline;
        // Caller holds the borrowed queue's Java submitLock for this native submit.
        s.submitted=true;unsafe=true;vkcheck(fn<PFN_vkQueueSubmit>("vkQueueSubmit")(queue,1,&si,VK_NULL_HANDLE),"input queue submit");
        hr(dx.queue->Wait(fence.Get(),s.ready),"D3D12 queue GPU wait");ID3D12CommandList* lists[]={s.commands.Get()};dx.queue->ExecuteCommandLists(1,lists);hr(dx.queue->Signal(fence.Get(),s.done),"D3D12 queue completion signal");unsafe=false;++submittedFrames;
        event("vulkan_d3d12_submit","realFrameId="+std::to_string(s.frameId)+" ready="+std::to_string(s.ready)+" completion="+std::to_string(s.done)+" cpuWait=false");
        event("slot_state","realFrameId="+std::to_string(s.frameId)+" state=GENERATING; output submitted by Vulkan worker with GPU timeline wait");
    }
    Bytes pixels(Readback& rb,uint32_t w,uint32_t h) {
        void* mapped{};D3D12_RANGE range{0,SIZE_T(rb.size)};hr(rb.buffer->Map(0,&range,&mapped),"sample map");Bytes out(size_t(w)*h*4);
        for(uint32_t y=0;y<h;++y)std::memcpy(out.data()+size_t(y)*w*4,static_cast<uint8_t*>(mapped)+rb.footprint.Offset+size_t(y)*rb.footprint.Footprint.RowPitch,size_t(w)*4);
        D3D12_RANGE none{0,0};rb.buffer->Unmap(0,&none);return out;
    }
    void release(Pool& p,Slot& s) {
        // Called only after the normal Vulkan output fence and all presentation fences
        // have completed. Their GPU wait on done proves the D3D12 work also completed.
        if(!s.leased||!s.submitted||fence->GetCompletedValue()<s.done||fence->GetCompletedValue()==UINT64_MAX)throw std::runtime_error("Premature lease retirement or device removed");
        hr(dx.device->GetDeviceRemovedReason(),"device removed reason");++completedFrames;
        void* mapped{};D3D12_RANGE range{0,16};hr(s.timingReadback->Map(0,&range,&mapped),"timing map");uint64_t ticks[2];std::memcpy(ticks,mapped,16);D3D12_RANGE none{0,0};s.timingReadback->Unmap(0,&none);
        hr(s.disableReadback->Map(0,&range,&mapped),"disable metadata map");auto disabled=*static_cast<uint32_t*>(mapped);s.disableReadback->Unmap(0,&none);if(disabled==0&&!s.reset)++generatedFrames;
        double inputMs=-1;
        if(p.inputTiming){uint64_t inputTicks[2]{};auto code=fn<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(device,p.inputTiming,s.timestampIndex,2,sizeof inputTicks,inputTicks,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT);vkcheck(code,"completed input timestamps");auto mask=vkTimestampBits==64?~uint64_t(0):((uint64_t(1)<<vkTimestampBits)-1);inputMs=double((inputTicks[1]-inputTicks[0])&mask)*vkPeriod/1e6;}
        event("completion","realFrameId="+std::to_string(s.frameId)+" done="+std::to_string(s.done)+" DLSSG_GPU_ms="+std::to_string(double(ticks[1]-ticks[0])*1000/dxFrequency)+" INPUT_GPU_COPY_ms="+std::to_string(inputMs)+" disable="+std::to_string(disabled)+" generatedFrames="+std::to_string(generatedFrames));
        if(s.sampled){auto b=pixels(s.colorSample),g=pixels(s.generatedSample);auto bh=sha(b),gh=sha(g);
            size_t black=0,sentinel=0;for(size_t j=0;j<g.size();j+=4){black+=g[j]==0&&g[j+1]==0&&g[j+2]==0;sentinel+=g[j]==255&&g[j+1]==0&&g[j+2]==255;}
            bool previousKnown=previousSampleId+1==s.frameId;bool valid=previousKnown&&!s.reset&&disabled==0&&gh!=bh&&gh!=previousSampleHash&&black<g.size()/4&&sentinel<g.size()/4;
            event("output_sample","realFrameId="+std::to_string(s.frameId)+" B_SHA256="+bh+" G_SHA256="+gh+" A_SHA256="+previousSampleHash+" previousKnown="+std::to_string(previousKnown)+" disable="+std::to_string(disabled)+" blackPixels="+std::to_string(black)+" sentinelPixels="+std::to_string(sentinel)+" valid="+std::to_string(valid));
            previousSampleHash=bh;previousSampleId=s.frameId;
            save(evidence.out/("sample-"+std::to_string(s.frameId)+"-B.bin"),b);save(evidence.out/("sample-"+std::to_string(s.frameId)+"-G.bin"),g);
            event("sample_metadata","realFrameId="+std::to_string(s.frameId)+" width="+std::to_string(p.width)+" height="+std::to_string(p.height)+" readback_api=Vulkan");
            events.flush();
        }
        s.leased=false;s.prepared=s.submitted=false;
        event("slot_recycle","realFrameId="+std::to_string(s.frameId)+" state=FREE");
        if(completedFrames%120==0)events.flush();
    }
    void closePool(Pool& p) {
        for(auto& s:p.slots)if(s->leased)throw std::runtime_error("Cannot destroy leased pool");
        if(p.feature){NVSDK_NGX_D3D12_ReleaseFeature(p.feature);p.feature=nullptr;}if(p.parameters){NVSDK_NGX_D3D12_DestroyParameters(p.parameters);p.parameters=nullptr;}
        for(auto& s:p.slots){for(auto& im:s->images){if(im.view)fn<PFN_vkDestroyImageView>("vkDestroyImageView")(device,im.view,nullptr);if(im.image)fn<PFN_vkDestroyImage>("vkDestroyImage")(device,im.image,nullptr);if(im.memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,im.memory,nullptr);im.view={};im.image={};im.memory={};im.dx.resource.Reset();}for(VkSample* sample:{&s->colorSample,&s->generatedSample}){if(sample->buffer)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,sample->buffer,nullptr);if(sample->memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,sample->memory,nullptr);}if(s->inputs){fn<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(device,commandPool,1,&s->inputs);s->inputs={};}}
        p.slots.clear();
        if(p.inputTiming){fn<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(device,p.inputTiming,nullptr);p.inputTiming={};}
    }
    void close() {
        evidence.raw("real_frames_submitted",std::to_string(submittedFrames));evidence.raw("real_frames_completed",std::to_string(completedFrames));evidence.raw("generated_disable_flag_zero_count",std::to_string(generatedFrames));
        events.flush();dx.debugMessages();evidence.raw("d3d12_debug_errors",std::to_string(validationErrors));
        if(unsafe){evidence.str("cleanup","UNSAFE_GPU_RETAINED_UNTIL_PROCESS_EXIT");return;}
        for(auto& p:pools)closePool(*p);
        if(timeline)fn<PFN_vkDestroySemaphore>("vkDestroySemaphore")(device,timeline,nullptr);
        if(commandPool)fn<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(device,commandPool,nullptr);
        dx.close();evidence.str("cleanup","DRAINED; borrowed Vulkan device retained");vendorLog.close();validationLog.close();
    }
};
static fs::path path(JNIEnv* e,jstring s){const jchar* chars=e->GetStringChars(s,nullptr);fs::path p(std::wstring(reinterpret_cast<const wchar_t*>(chars),e->GetStringLength(s)));e->ReleaseStringChars(s,chars);return p;}
static void error(JNIEnv* e,const std::exception& ex){e->ThrowNew(e->FindClass("java/lang/IllegalStateException"),ex.what());}
static Session& session(jlong h){if(!h)throw std::runtime_error("Null session");return *reinterpret_cast<Session*>(h);}
static Pool& pool(jlong h){if(!h)throw std::runtime_error("Null pool");return *reinterpret_cast<Pool*>(h);}
static Slot& slot(Pool& p,jint i){if(i<0||size_t(i)>=p.slots.size())throw std::runtime_error("Slot out of range");return *p.slots[i];}
}
using namespace integration;
#define JNI_METHOD(name) Java_org_ireallywanttosleep_wisteria_dlssg_DlssgBridge_##name
extern "C" JNIEXPORT jint JNICALL JNI_METHOD(abi)(JNIEnv*,jclass){return 1;}
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(create)(JNIEnv* e,jclass,jlong i,jlong p,jlong d,jlong q,jint family,jlong proc,jstring out,jstring dll,jstring runtime){
    Session* s{};try{s=new Session(path(e,out),path(e,dll),path(e,runtime));s->borrow(reinterpret_cast<VkInstance>(i),reinterpret_cast<VkPhysicalDevice>(p),reinterpret_cast<VkDevice>(d),reinterpret_cast<VkQueue>(q),family,reinterpret_cast<PFN_vkGetInstanceProcAddr>(proc));return reinterpret_cast<jlong>(s);}catch(const std::exception& ex){if(s){s->unsafe=true;s->evidence.str("integration_error",ex.what());}error(e,ex);return 0;}
}
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(createPool)(JNIEnv* e,jclass,jlong handle,jint w,jint h,jint rw,jint rh,jint capacity){try{return reinterpret_cast<jlong>(session(handle).createPool(w,h,rw,rh,capacity));}catch(const std::exception& ex){error(e,ex);return 0;}}
extern "C" JNIEXPORT jlongArray JNICALL JNI_METHOD(image)(JNIEnv* e,jclass,jlong ph,jint index,jint role){try{auto& im=slot(pool(ph),index).images.at(role);jlong values[]={reinterpret_cast<jlong>(im.image),reinterpret_cast<jlong>(im.view),reinterpret_cast<jlong>(im.memory)};auto a=e->NewLongArray(3);e->SetLongArrayRegion(a,0,3,values);return a;}catch(const std::exception& ex){error(e,ex);return nullptr;}}
extern "C" JNIEXPORT jlongArray JNICALL JNI_METHOD(prepare)(JNIEnv* e,jclass,jlong h,jlong ph,jint index,jlong command,jlongArray sources,jfloatArray data,jlong frameId,jdouble delta,jboolean reset,jboolean flip,jboolean sample){try{std::vector<jlong> src(e->GetArrayLength(sources));e->GetLongArrayRegion(sources,0,jint(src.size()),src.data());std::vector<float> constants(e->GetArrayLength(data));e->GetFloatArrayRegion(data,0,jint(constants.size()),constants.data());auto& s=session(h);auto& p=pool(ph);auto& sl=slot(p,index);s.prepare(p,sl,reinterpret_cast<VkCommandBuffer>(command),src,constants,frameId,delta,reset,flip,sample);jlong result[]={reinterpret_cast<jlong>(s.timeline),jlong(sl.done)};auto a=e->NewLongArray(2);e->SetLongArrayRegion(a,0,2,result);return a;}catch(const std::exception& ex){error(e,ex);return nullptr;}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(submit)(JNIEnv* e,jclass,jlong h,jlong ph,jint index,jlongArray waits){try{std::vector<jlong> values(e->GetArrayLength(waits));e->GetLongArrayRegion(waits,0,jint(values.size()),values.data());session(h).submit(slot(pool(ph),index),values);}catch(const std::exception& ex){session(h).unsafe=true;error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(release)(JNIEnv* e,jclass,jlong h,jlong ph,jint index){try{auto& p=pool(ph);session(h).release(p,slot(p,index));}catch(const std::exception& ex){session(h).unsafe=true;error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(abort)(JNIEnv* e,jclass,jlong h,jlong ph,jint index){try{auto& sl=slot(pool(ph),index);if(sl.submitted){session(h).unsafe=true;throw std::runtime_error("Abort after GPU submission retains native session");}for(size_t j=0;j<5;++j){sl.images[j].initialized=sl.priorInitialized[j];sl.images[j].dx.state=D3D12_RESOURCE_STATE_COMMON;}sl.leased=sl.prepared=false;}catch(const std::exception& ex){error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(closePool)(JNIEnv* e,jclass,jlong h,jlong ph){try{session(h).closePool(pool(ph));}catch(const std::exception& ex){error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(close)(JNIEnv* e,jclass,jlong h){try{auto& s=session(h);s.close();if(!s.unsafe)delete &s;}catch(const std::exception& ex){session(h).unsafe=true;error(e,ex);}}
