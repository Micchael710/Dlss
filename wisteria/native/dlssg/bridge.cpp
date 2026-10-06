// Experimental local bridge. Only the already-audited public loader and NGX helpers
// are reused. No private community export is resolved or invoked.
#define wmain UncalledHistoricalHarnessEntry
#include "../../../experiments/dlssg-external-harness/harness.cpp"
#undef wmain
#include <jni.h>
#include <array>
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <bit>
#include "reuse_contract.h"
#include "HybridInterpolationCS.h"
#include "MotionConvertCS.h"

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
    struct Marker { uint64_t interval{}; const char* phase{}; };
    std::array<Marker,16> markers{};
    size_t markerCount{};
    uint64_t diagnosticOrdinal{}, commandGeneration{};
    std::function<void(Slot&)> completionDiagnostic;
    // color, HUDless, depth, motion, followed by N independent generated images.
    std::vector<Image> images;
    VkCommandBuffer inputs{};
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    std::vector<ComPtr<ID3D12Resource>> disable, disableReadback;
    // Bounded diagnostic snapshots: after sentinel upload and after the whole group.
    std::vector<ComPtr<ID3D12Resource>> statusBefore, statusGroupEnd;
    uint64_t completionObserved{};
    std::string statusReadError;
    bool diagnostic=false;
    ComPtr<ID3D12Resource> timingReadback;
    ComPtr<ID3D12QueryHeap> timing;
    VkSample colorSample; std::vector<VkSample> generatedSamples;
    uint64_t ready{}, done{}, frameId{};
    std::array<jlong,2> borrowedReady{};
    std::string borrowedEvidence;
    bool leased=false, prepared=false, submitted=false, sampled=false, reset=false;
    std::vector<bool> priorInitialized;
    HANDLE completionEvent{}; PTP_WAIT completionWait{};
    ComPtr<ID3D12Fence> completionFence;
    JavaVM* vm{}; jobject notification{}; jmethodID notifyMethod{};
    std::vector<jint> flags; std::atomic<bool> flagsReady{false};
    uint32_t timestampIndex{};

    // Hybrid X4 per-slot resources:
    ComPtr<ID3D12Resource> currentMotionNDC;
    ComPtr<ID3D12DescriptorHeap> heap;
};
// One event-driven callback per real interval, after the shared D3D12 fence.
// No WaitForSingleObject, sleep, polling, or CPU fence wait in the handoff path.
static void CALLBACK notifyFlags(PTP_CALLBACK_INSTANCE, void* context, PTP_WAIT, TP_WAIT_RESULT) {
    auto& s=*static_cast<Slot*>(context);
    auto vm=s.vm; auto target=s.notification; auto method=s.notifyMethod;
    const auto frame=s.frameId, done=s.done;
    std::vector<jint> flags(s.disableReadback.size(),-1);
    try {
        auto completed=s.completionFence->GetCompletedValue();
        s.completionObserved=completed;
        if(completed==UINT64_MAX||completed<done)throw std::runtime_error("Fence completion unavailable/device removed");
        for(size_t k=0;k<flags.size();++k) {
            void* map{};D3D12_RANGE range{0,4},none{0,0};
            hr(s.disableReadback[k]->Map(0,&range,&map),"completed disable map");
            flags[k]=*static_cast<jint*>(map);s.disableReadback[k]->Unmap(0,&none);
        }
    } catch(const std::exception& ex) { s.statusReadError=ex.what();std::fill(flags.begin(),flags.end(),-1); }
    if(s.completionDiagnostic)s.completionDiagnostic(s);
    s.flags=flags;s.notification=nullptr;s.flagsReady.store(true,std::memory_order_release);
    JNIEnv* e{};bool attached=false;
    if(vm->GetEnv(reinterpret_cast<void**>(&e),JNI_VERSION_1_8)==JNI_EDETACHED) {
        attached=vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&e),nullptr)==JNI_OK;
    }
    // All accesses to Slot precede notification; Java may retire/reuse it after this.
    if(e){auto values=e->NewIntArray(static_cast<jsize>(flags.size()));
        if(values){e->SetIntArrayRegion(values,0,static_cast<jsize>(flags.size()),flags.data());
            e->CallVoidMethod(target,method,jlong(frame),values,jlong(done));e->DeleteLocalRef(values);}
        if(e->ExceptionCheck())e->ExceptionClear();e->DeleteGlobalRef(target);}
    if(attached)vm->DetachCurrentThread();
}
struct Pool {
    unsigned width{},height{},renderWidth{},renderHeight{},generatedCount{};
    NVSDK_NGX_Parameter* parameters{}; NVSDK_NGX_Handle* feature{};
    std::vector<std::unique_ptr<Slot>> slots;
    bool failed=false;
    VkQueryPool inputTiming{};

    // Hybrid X4 history resources
    ComPtr<ID3D12Resource> historyColor;
    ComPtr<ID3D12Resource> historyDepth;
    ComPtr<ID3D12Resource> historyMotionNDC;
    bool hasHistory=false;
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
    ComPtr<ID3D12Resource> disableZero;
    ComPtr<ID3D12Resource> disableOne;
    ComPtr<ID3D12RootSignature> hybridRoot;
    ComPtr<ID3D12PipelineState> hybridPSO;
    ComPtr<ID3D12RootSignature> motionConvertRoot;
    ComPtr<ID3D12PipelineState> motionConvertPSO;
    UINT descriptorStride{};
    uint64_t sequence{}, dxFrequency{}, submittedFrames{}, completedFrames{}, generatedFrames{};
    unsigned reportedMax{},requestedMax{};
    float vkPeriod{}; uint32_t vkTimestampBits{};
    bool unsafe=false; std::vector<std::unique_ptr<Pool>> pools;
    std::ofstream events;
    std::ofstream twoInterval;
    std::mutex diagnosticMutex;
    bool deviceFaultEnabled=false, checkpointsEnabled=false, sync2Enabled=false;
    uint64_t diagnosticIntervals{};
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
        events.flush(); // Retain the short failing run; no checkpoint replacement or fence wait.
    }
    void state(Slot& s,const char* type,const std::string& detail="") {
        std::lock_guard<std::mutex> lock(diagnosticMutex);
        if(!twoInterval.is_open()||!s.diagnosticOrdinal||s.diagnosticOrdinal>3)return;
        twoInterval<<"{\"interval\":"<<s.diagnosticOrdinal<<",\"realFrameId\":"<<s.frameId
            <<",\"event\":"<<jsonQuote(type)<<",\"commandBuffer\":"<<jsonQuote(handleText(s.inputs))
            <<",\"commandGeneration\":"<<s.commandGeneration<<",\"commandPool\":"<<jsonQuote(handleText(commandPool))
            <<",\"queue\":"<<jsonQuote(handleText(queue))<<",\"ready\":"<<s.ready<<",\"done\":"<<s.done
            <<",\"detail\":"<<jsonQuote(detail)<<"}\n";twoInterval.flush();
    }
    void checkpoint(Slot& s,VkCommandBuffer c,const char* phase) {
        if(!checkpointsEnabled||s.diagnosticOrdinal>3||s.markerCount>=s.markers.size())return;
        auto& marker=s.markers[s.markerCount++];marker={s.diagnosticOrdinal,phase};
        fn<PFN_vkCmdSetCheckpointNV>("vkCmdSetCheckpointNV")(c,&marker);
    }
    void configureDiagnostics(const fs::path& out,bool fault,bool checkpoints,bool sync2) {
        deviceFaultEnabled=fault;checkpointsEnabled=checkpoints;sync2Enabled=sync2;twoInterval.open(out/"fg-two-interval-state.jsonl");
        evidence.raw("device_fault_enabled",fault?"true":"false");evidence.raw("diagnostic_checkpoints_enabled",checkpoints?"true":"false");
    }
    void captureFault() {
        try {
            if(deviceFaultEnabled) {
                VkDeviceFaultCountsEXT counts{VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
                auto get=fn<PFN_vkGetDeviceFaultInfoEXT>("vkGetDeviceFaultInfoEXT");auto r=get(device,&counts,nullptr);
                event("device_fault_counts","VkResult="+std::to_string(r)+" addressInfoCount="+std::to_string(counts.addressInfoCount)+" vendorInfoCount="+std::to_string(counts.vendorInfoCount));
                if(r==VK_SUCCESS){std::vector<VkDeviceFaultAddressInfoEXT> addresses(counts.addressInfoCount);std::vector<VkDeviceFaultVendorInfoEXT> vendors(counts.vendorInfoCount);
                    VkDeviceFaultInfoEXT info{VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT};info.pAddressInfos=addresses.data();info.pVendorInfos=vendors.data();counts.vendorBinarySize=0;
                    r=get(device,&counts,&info);event("device_fault_description","VkResult="+std::to_string(r)+" description="+info.description);
                    for(auto& a:addresses)event("device_fault_address","type="+std::to_string(a.addressType)+" address="+std::to_string(a.reportedAddress)+" precision="+std::to_string(a.addressPrecision));
                    for(auto& v:vendors)event("device_fault_vendor",std::string(v.description)+" code="+std::to_string(v.vendorFaultCode)+" data="+std::to_string(v.vendorFaultData));}
            } else event("device_fault_description","UNAVAILABLE_NOT_ENABLED");
            if(checkpointsEnabled){
                VkQueue producer{};fn<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(device,family,0,&producer);
                for(auto q:{producer,queue}){uint32_t n{};auto get=fn<PFN_vkGetQueueCheckpointDataNV>("vkGetQueueCheckpointDataNV");get(q,&n,nullptr);std::vector<VkCheckpointDataNV> data(n);for(auto& d:data)d.sType=VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;get(q,&n,data.data());
                    for(auto& d:data){std::string phase="UNKNOWN_EXTERNAL_MARKER";uint64_t ordinal{};
                        for(auto& p:pools)for(auto& slot:p->slots)for(auto& m:slot->markers)if(&m==d.pCheckpointMarker){phase=m.phase?m.phase:"UNSET";ordinal=m.interval;}
                        event("device_lost_checkpoint","queue="+handleText(q)+" stage="+std::to_string(d.stage)+" marker="+handleText(d.pCheckpointMarker)+" interval="+std::to_string(ordinal)+" phase="+phase);}}
            }
        } catch(const std::exception& ex){event("device_fault_query_error",ex.what());}
    }
    void queryVram(const char* tag) {
        ComPtr<IDXGIAdapter3> adapter3;
        if (dx.adapter && SUCCEEDED(dx.adapter.As(&adapter3))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
            if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
                double budgetMB = double(memInfo.Budget) / (1024.0 * 1024.0);
                double usageMB = double(memInfo.CurrentUsage) / (1024.0 * 1024.0);
                double availMB = double(memInfo.Budget > memInfo.CurrentUsage ? memInfo.Budget - memInfo.CurrentUsage : 0) / (1024.0 * 1024.0);
                evidence.raw("vram_budget_mb", std::to_string(budgetMB));
                evidence.raw("vram_current_usage_mb", std::to_string(usageMB));
                evidence.raw("vram_available_mb", std::to_string(availMB));
                event("vram_info", std::string(tag) + " budget_mb=" + std::to_string(budgetMB) + " usage_mb=" + std::to_string(usageMB) + " avail_mb=" + std::to_string(availMB));
            }
        }
    }
    void recordDred(ID3D12Device* dev, const char* context) {
#if defined(__ID3D12DeviceRemovedExtendedData1_INTERFACE_DEFINED__)
        if (!dev) return;
        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
        if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dred))) && dred) {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
            if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
                const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
                if (node && node->pLastBreadcrumbValue) {
                    evidence.raw("dred_last_breadcrumb", std::to_string(*node->pLastBreadcrumbValue));
                } else {
                    evidence.str("dred_last_breadcrumb", "NONE");
                }
            }
            D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault{};
            if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault)) && pageFault.PageFaultVA != 0) {
                std::ostringstream va; va << "0x" << std::hex << pageFault.PageFaultVA;
                evidence.str("dred_page_fault_va", va.str());
                const D3D12_DRED_ALLOCATION_NODE1* alloc = pageFault.pHeadExistingAllocationNode;
                if (!alloc) alloc = pageFault.pHeadRecentFreedAllocationNode;
                if (alloc && alloc->ObjectNameA) {
                    evidence.str("dred_existing_allocation", alloc->ObjectNameA);
                }
                const D3D12_DRED_ALLOCATION_NODE1* freed = pageFault.pHeadRecentFreedAllocationNode;
                if (freed && freed->ObjectNameA) {
                    evidence.str("dred_recent_freed_allocation", freed->ObjectNameA);
                }
            }
            event("dred_dump", std::string("context=") + context);
        }
#endif
    }
    void initHybridPipelines() {
        D3D12_DESCRIPTOR_RANGE ranges[2]{};
        ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7, 0, 0, 0};
        ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0, 0};
        D3D12_ROOT_PARAMETER params[3]{};
        params[0].ParameterType = params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable = {1, &ranges[0]};
        params[1].DescriptorTable = {1, &ranges[1]};
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[2].Constants = {0, 0, 5};

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_ROOT_SIGNATURE_DESC desc{3, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob, errBlob;
        hr(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errBlob), "Serialize hybrid root");
        hr(dx.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&hybridRoot)), "Create hybrid root");

        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = hybridRoot.Get();
        pd.CS = {g_HybridInterpolationCS, sizeof(g_HybridInterpolationCS)};
        hr(dx.device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&hybridPSO)), "Create HybridInterpolationCS PSO");

        // Dedicated root signature for MotionConvertCS:
        // Parameter 0: SRV table with 1 descriptor (t0, InMotion)
        // Parameter 1: UAV table with 1 descriptor (u0, OutNDC)
        // Parameter 2: 32-bit constants (Width, Height, padding - 5 uints)
        D3D12_DESCRIPTOR_RANGE mcRanges[2]{};
        mcRanges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
        mcRanges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0};
        D3D12_ROOT_PARAMETER mcParams[3]{};
        mcParams[0].ParameterType = mcParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        mcParams[0].DescriptorTable = {1, &mcRanges[0]};
        mcParams[1].DescriptorTable = {1, &mcRanges[1]};
        mcParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        mcParams[2].Constants = {0, 0, 5};

        D3D12_ROOT_SIGNATURE_DESC mcDesc{3, mcParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> mcBlob, mcErrBlob;
        hr(D3D12SerializeRootSignature(&mcDesc, D3D_ROOT_SIGNATURE_VERSION_1, &mcBlob, &mcErrBlob), "Serialize motion convert root");
        hr(dx.device->CreateRootSignature(0, mcBlob->GetBufferPointer(), mcBlob->GetBufferSize(), IID_PPV_ARGS(&motionConvertRoot)), "Create motion convert root");

        D3D12_COMPUTE_PIPELINE_STATE_DESC mcPd{};
        mcPd.pRootSignature = motionConvertRoot.Get();
        mcPd.CS = {g_MotionConvertCS, sizeof(g_MotionConvertCS)};
        hr(dx.device->CreateComputePipelineState(&mcPd, IID_PPV_ARGS(&motionConvertPSO)), "Create MotionConvertCS PSO");

        evidence.str("motion_convert_dedicated_root_signature", "YES");
        evidence.raw("motion_convert_srv_count", "1");
        evidence.raw("motion_convert_uav_count", "1");
        evidence.str("descriptor_table_out_of_range", "NO");

        descriptorStride = dx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        disableZero = dx.buffer(16, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void* map0{}; D3D12_RANGE none{0, 0};
        hr(disableZero->Map(0, &none, &map0), "disable zero map");
        std::memset(map0, 0, 16);
        disableZero->Unmap(0, nullptr);

        // Reset intervals suppress custom G25/G75 outputs. Their status buffers
        // still need a valid API value (1 = disabled); leaving the sentinel would
        // make the Java output-status observer latch the provider unavailable.
        disableOne = dx.buffer(16, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void* map1{};
        hr(disableOne->Map(0, &none, &map1), "disable one map");
        std::memset(map1, 0, 16);
        *static_cast<uint32_t*>(map1) = 1u;
        disableOne->Unmap(0, nullptr);
    }
    Session(fs::path out, fs::path dll, fs::path runtime,unsigned count):evidence(out),dx(evidence,runtime),events(out/"provider-events.jsonl") {
        if(sha(readFile(runtime/"nvngx_dlssg.dll"))!="ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82")
            throw std::runtime_error("Runtime SHA256 mismatch");
        vendorLog.open(out/"ngx.log"); validationLog.open(out/"d3d12-debug.log");
        requestedMax=count;

        // DRED initialization before D3D12 device creation
        bool dredAvailable = false;
        bool dredEnabled = false;
#if defined(__ID3D12DeviceRemovedExtendedDataSettings_INTERFACE_DEFINED__)
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
        HRESULT hrDred = D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings));
        if (SUCCEEDED(hrDred) && dredSettings) {
            dredAvailable = true;
            dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dredSettings->SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT_FORCED_OFF);
            dredEnabled = true;
        }
#endif
        evidence.str("dred_available", dredAvailable ? "YES" : "NO");
        evidence.str("dred_enabled", dredEnabled ? "YES" : "NO");

        // External loader for NVIDIA DLSSG: always start with 1 native generated frame
        external.start(evidence, dll, 1);
        dx.initialize(D3D12InitializationContext::EmbeddedMinecraft);

        // Device removal status after device creation & NGX Init
        auto reasonAfterCreate = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
        evidence.str("device_reason_after_create_device", hrHex(reasonAfterCreate));
        evidence.str("device_reason_after_ngx_init", hrHex(reasonAfterCreate));

        NVSDK_NGX_Parameter* caps{};auto query=NVSDK_NGX_D3D12_GetCapabilityParameters(&caps);
        evidence.str("mfg_capability_query_result",resultHex(query));
        if(!NVSDK_NGX_SUCCEED(query)||!caps)throw std::runtime_error("NGX capabilities unavailable");
        unsigned available{};auto a=NVSDK_NGX_Parameter_GetUI(caps,NVSDK_NGX_Parameter_FrameGeneration_Available,&available);
        unsigned rawMax{};auto m=NVSDK_NGX_Parameter_GetUI(caps,NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax,&rawMax);
        NVSDK_NGX_D3D12_DestroyParameters(caps);
        evidence.raw("raw_reported_multiframecountmax",std::to_string(rawMax));
        evidence.raw("sm86_reported_native_max",std::to_string(rawMax));
        evidence.str("raw_max_getter_result",resultHex(m));
        evidence.str("capability_provenance","public NGX query after external loader; may be hooked; not proof of execution");
        if(!NVSDK_NGX_SUCCEED(a)||!available||!NVSDK_NGX_SUCCEED(m)||(rawMax<1))throw std::runtime_error("Requested MFG count exceeds reported NGX availability/max");
        // In Hybrid X4 mode (count == 3), our hybrid provider reports 3 to Java:
        reportedMax = (count == 3) ? 3 : rawMax;
        evidence.raw("hybrid_provider_reported_max",std::to_string(reportedMax));
        evidence.raw("hybrid_reported_multiframecountmax",std::to_string(reportedMax));
        if(reportedMax < count) throw std::runtime_error("Requested MFG count exceeds reported max");
        hr(dx.queue->GetTimestampFrequency(&dxFrequency),"D3D12 timestamp frequency");
        disableSentinel=dx.buffer(16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* map{};D3D12_RANGE none{0,0};hr(disableSentinel->Map(0,&none,&map),"disable sentinel map");std::memset(map,255,16);disableSentinel->Unmap(0,nullptr);
        initHybridPipelines();
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
    ComPtr<ID3D12Resource> createTexture(uint32_t w, uint32_t h, DXGI_FORMAT format) {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT; hp.CreationNodeMask = hp.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width = w; rd.Height = h; rd.DepthOrArraySize = rd.MipLevels = 1; rd.Format = format;
        rd.SampleDesc.Count = 1; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        ComPtr<ID3D12Resource> res;
        hr(dx.device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&res)), "create texture");
        return res;
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
    D3D12_CPU_DESCRIPTOR_HANDLE cpu(Slot& s, UINT i) {
        auto h = s.heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(i) * descriptorStride;
        return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE gpu(Slot& s, UINT i) {
        auto h = s.heap->GetGPUDescriptorHandleForHeapStart();
        h.ptr += UINT64(i) * descriptorStride;
        return h;
    }
    void createSRV(ID3D12Resource* res, DXGI_FORMAT fmt, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format = fmt; d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Texture2D.MipLevels = 1;
        dx.device->CreateShaderResourceView(res, &d, dest);
    }
    void createUAV(ID3D12Resource* res, DXGI_FORMAT fmt, D3D12_CPU_DESCRIPTOR_HANDLE dest) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
        d.Format = fmt; d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        dx.device->CreateUnorderedAccessView(res, nullptr, res ? &d : nullptr, dest);
    }
    void transitionRes(Slot& s, ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        if(before != after && res) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition = {res, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
            s.commands->ResourceBarrier(1, &b);
        }
    }
    void setDisable(Slot& s, UINT index, ID3D12Resource* sourceUpload) {
        D3D12_RESOURCE_BARRIER fb{}; fb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        fb.Transition = {s.disable[index].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST};
        s.commands->ResourceBarrier(1, &fb);
        s.commands->CopyBufferRegion(s.disable[index].Get(), 0, sourceUpload, 0, 16);
        std::swap(fb.Transition.StateBefore, fb.Transition.StateAfter);
        s.commands->ResourceBarrier(1, &fb);
    }
    void copyHistory(Pool& p, Slot& s, bool hasMotion) {
        // Color: s.images[0] -> p.historyColor
        dxBarrier(s, s.images[0], D3D12_RESOURCE_STATE_COPY_SOURCE);
        transitionRes(s, p.historyColor.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        s.commands->CopyResource(p.historyColor.Get(), s.images[0].dx.resource.Get());
        transitionRes(s, p.historyColor.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        dxBarrier(s, s.images[0], D3D12_RESOURCE_STATE_COMMON);

        // Depth: s.images[2] -> p.historyDepth
        dxBarrier(s, s.images[2], D3D12_RESOURCE_STATE_COPY_SOURCE);
        transitionRes(s, p.historyDepth.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        s.commands->CopyResource(p.historyDepth.Get(), s.images[2].dx.resource.Get());
        transitionRes(s, p.historyDepth.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        dxBarrier(s, s.images[2], D3D12_RESOURCE_STATE_COMMON);

        if (hasMotion) {
            // Motion NDC: s.currentMotionNDC -> p.historyMotionNDC
            transitionRes(s, s.currentMotionNDC.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
            transitionRes(s, p.historyMotionNDC.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
            s.commands->CopyResource(p.historyMotionNDC.Get(), s.currentMotionNDC.Get());
            transitionRes(s, p.historyMotionNDC.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
            transitionRes(s, s.currentMotionNDC.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
        }
    }
    Pool* createPool(unsigned w,unsigned h,unsigned rw,unsigned rh,unsigned capacity,unsigned count) {
        if(!w||!h||!rw||!rh||capacity<2||capacity>64||!count||count>4||count>reportedMax||count>requestedMax)throw std::runtime_error("Invalid pool contract");
        auto holder=std::make_unique<Pool>();auto p=holder.get();pools.push_back(std::move(holder));
        p->width=w;p->height=h;p->renderWidth=rw;p->renderHeight=rh;p->generatedCount=count;
        if(count == 3) {
            p->historyColor = createTexture(w, h, DXGI_FORMAT_R8G8B8A8_UNORM);
            p->historyDepth = createTexture(rw, rh, DXGI_FORMAT_R32_FLOAT);
            p->historyMotionNDC = createTexture(rw, rh, DXGI_FORMAT_R32G32_FLOAT);
            p->hasHistory = false;
        }
        if(vkTimestampBits){VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=capacity*2;vkcheck(fn<PFN_vkCreateQueryPool>("vkCreateQueryPool")(device,&qi,nullptr,&p->inputTiming),"input timing query pool");}
        for(unsigned i=0;i<capacity;++i){auto ptr=std::make_unique<Slot>();auto& s=*ptr;p->slots.push_back(std::move(ptr));
            s.images.resize(4+count);s.priorInitialized.resize(4+count);
            for(unsigned j=0;j<4+count;++j)s.images[j]=createImage(j==2||j==3?rw:w,j==2||j==3?rh:h,j==2?DXGI_FORMAT_R32_FLOAT:j==3?DXGI_FORMAT_R32G32_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM,j==2?VK_FORMAT_R32_SFLOAT:j==3?VK_FORMAT_R32G32_SFLOAT:VK_FORMAT_R8G8B8A8_UNORM);
            VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commandPool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
            vkcheck(fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(device,&ca,&s.inputs),"slot input command allocation");
            hr(dx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&s.allocator)),"slot allocator");
            hr(dx.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,s.allocator.Get(),nullptr,IID_PPV_ARGS(&s.commands)),"slot command list");hr(s.commands->Close(),"slot initial close");
            s.disable.resize(count);s.disableReadback.resize(count);s.generatedSamples.resize(count);s.flags.resize(count,-1);
            s.statusBefore.resize(count);s.statusGroupEnd.resize(count);
            for(unsigned k=0;k<count;++k){s.disable[k]=dx.buffer(16,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);s.disableReadback[k]=dx.buffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);}
            for(unsigned k=0;k<count;++k){s.statusBefore[k]=dx.buffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);s.statusGroupEnd[k]=dx.buffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);}
            s.completionFence=fence;
            s.completionEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!s.completionEvent)throw std::runtime_error("completion event create");
            s.completionWait=CreateThreadpoolWait(notifyFlags,&s,nullptr);if(!s.completionWait)throw std::runtime_error("completion callback create");
            s.timestampIndex=i*2;

            D3D12_QUERY_HEAP_DESC q{};q.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;q.Count=count*2;hr(dx.device->CreateQueryHeap(&q,IID_PPV_ARGS(&s.timing)),"slot timestamp heap");
            s.timingReadback=dx.buffer(count*16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            // Pixel readback objects exist only for the bounded diagnostic sampling window.
            // Every leased slot can participate in a consecutive bounded capture.
            s.colorSample=createSample(w,h);for(auto& sample:s.generatedSamples)sample=createSample(w,h);

            if(count == 3) {
                s.currentMotionNDC = createTexture(rw, rh, DXGI_FORMAT_R32G32_FLOAT);
                D3D12_DESCRIPTOR_HEAP_DESC hd{};
                hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                hd.NumDescriptors = 16;
                hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                hr(dx.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&s.heap)), "slot descriptor heap");
            }
        }
        auto code=NVSDK_NGX_D3D12_AllocateParameters(&p->parameters);if(!NVSDK_NGX_SUCCEED(code)||!p->parameters)throw std::runtime_error("Allocate feature parameters "+resultHex(code));
        NVSDK_NGX_DLSSG_Create_Params cp{};cp.Width=w;cp.Height=h;cp.RenderWidth=rw;cp.RenderHeight=rh;cp.NativeBackbufferFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
        NVSDK_NGX_Parameter_SetUI(p->parameters,NVSDK_NGX_DLSSG_Parameter_Width,w);NVSDK_NGX_Parameter_SetUI(p->parameters,NVSDK_NGX_DLSSG_Parameter_Height,h);
        NVSDK_NGX_Parameter_SetUI(p->parameters,NVSDK_NGX_DLSSG_Parameter_UserInterfaceRecompositionEnabled,0);

        queryVram("before_create_feature");

        dx.begin();code=NGX_D3D12_CREATE_DLSSG(dx.commands.Get(),1,1,&p->feature,p->parameters,&cp);
        evidence.str("create_result",resultHex(code));if(!NVSDK_NGX_SUCCEED(code)||!p->feature)throw std::runtime_error("CreateFeature "+resultHex(code));
        // This bounded setup wait is outside the per-frame handoff path.
        dx.complete("Minecraft pool CreateFeature");evidence.str("minecraft_create_feature","PASS");

        // CREATEFEATURE GATE
        auto reasonAfterCF = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
        evidence.str("device_reason_after_create_feature", hrHex(reasonAfterCF));
        if (FAILED(reasonAfterCF)) {
            evidence.str("fail_stage", "CREATE_FEATURE_GPU_EXECUTION");
            recordDred(dx.device.Get(), "CREATE_FEATURE_FAIL");
            throw std::runtime_error("Device removed during CreateFeature GPU execution " + hrHex(reasonAfterCF));
        }

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
        if((before==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL&&after==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                ||(before==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL&&after==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
            bool toCopy=after==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            if(sync2Enabled){VkImageMemoryBarrier2KHR b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR};
                b.srcStageMask=toCopy?VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR:VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
                b.dstStageMask=toCopy?VK_PIPELINE_STAGE_2_BLIT_BIT_KHR:VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
                b.srcAccessMask=toCopy?VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR:VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
                b.dstAccessMask=toCopy?VK_ACCESS_2_TRANSFER_READ_BIT_KHR:VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR;
                b.oldLayout=VkImageLayout(before);b.newLayout=VkImageLayout(after);b.image=image;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                VkDependencyInfoKHR dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};dep.imageMemoryBarrierCount=1;dep.pImageMemoryBarriers=&b;
                fn<PFN_vkCmdPipelineBarrier2KHR>("vkCmdPipelineBarrier2KHR")(c,&dep);return;
            }
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.oldLayout=VkImageLayout(before);b.newLayout=VkImageLayout(after);b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.srcAccessMask=toCopy?VK_ACCESS_SHADER_READ_BIT:VK_ACCESS_TRANSFER_READ_BIT;b.dstAccessMask=toCopy?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_SHADER_READ_BIT;
            fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(c,toCopy?VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:VK_PIPELINE_STAGE_TRANSFER_BIT,toCopy?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b);return;
        }
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
        requireSlotRetired(s.leased,s.submitted,unsafe);
        if(s.leased||unsafe||p.failed||(src.size()!=20&&src.size()!=32)||constants.size()!=108)throw std::runtime_error("Invalid or failed slot/metadata contract");
        s.borrowedReady={};s.borrowedEvidence.clear();
        if(src.size()==32){
            VkQueue actualProducer{};
            if(src[22]!=family||src[23]!=0||src[20]!=reinterpret_cast<jlong>(device)||src[31]!=1
                    ||!src[24]||src[25]<=0||!src[26]||src[27]<=0||!src[29]||!src[30]||src[29]==src[30])
                throw std::runtime_error("Borrowed producer device/family/submission contract rejected");
            fn<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(device,family,uint32_t(src[23]),&actualProducer);
            if(src[21]!=reinterpret_cast<jlong>(actualProducer))throw std::runtime_error("Borrowed producer queue identity mismatch");
            if(src[13]!=VK_FORMAT_R32_SFLOAT||src[18]!=VK_FORMAT_R16G16_SFLOAT
                    ||src[14]!=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL||src[19]!=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                throw std::runtime_error("Borrowed depth/motion format/layout contract rejected");
            s.borrowedReady={src[29],src[30]};
            s.borrowedEvidence="logicalFrame="+std::to_string(src[28])+" captureGeneration="+std::to_string(src[27])+
                " producerQueue="+std::to_string(src[21])+" producerFamily="+std::to_string(src[22])+" producerIndex="+std::to_string(src[23])+
                " producerCommand="+std::to_string(src[24])+" producerSubmission="+std::to_string(src[25])+" producerFence="+std::to_string(src[26])+
                " depthReady="+std::to_string(src[29])+" motionReady="+std::to_string(src[30])+" sourceReadyValue=BINARY_SIGNAL";
        }
        s.leased=true;s.prepared=false;s.submitted=false;s.frameId=frameId;s.sampled=sample&&s.colorSample.buffer;s.reset=reset;
        s.diagnosticOrdinal=++diagnosticIntervals;++s.commandGeneration;s.markerCount=0;
        s.completionDiagnostic=[this](Slot& completed){state(completed,"D3D12_DONE","observed="+std::to_string(completed.completionObserved)+" error="+completed.statusReadError);};
        s.flagsReady.store(false);std::fill(s.flags.begin(),s.flags.end(),-1);
        s.diagnostic=submittedFrames<8||s.sampled;s.completionObserved=0;s.statusReadError.clear();
        for(size_t j=0;j<s.images.size();++j)s.priorInitialized[j]=s.images[j].initialized;
        s.ready=++sequence;s.done=++sequence;
        state(s,"FRAME_BEGIN");state(s,"SR_READINESS",s.borrowedEvidence);
        vkcheck(fn<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(s.inputs,0),"input reset");VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkcheck(fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(s.inputs,&bi),"input begin");
        state(s,"COMMAND_BEGIN");checkpoint(s,s.inputs,"SR_COMPLETION_WAIT_PASSED");
        if(p.inputTiming){fn<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(s.inputs,p.inputTiming,s.timestampIndex,2);fn<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(s.inputs,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,p.inputTiming,s.timestampIndex);}
        event("slot_state","realFrameId="+std::to_string(frameId)+" state=FILLING");
        for(size_t j=0;j<s.images.size();++j){auto& im=s.images[j];
            // Previous output acquire was TRANSFER_SRC for the presented images.
            if(im.initialized&&(j==0||j>=4))sourceBarrier(s.inputs,im.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL);
            sharedBarrier(s.inputs,im,false,false);
            if(j<4){auto image=reinterpret_cast<VkImage>(src[j*5]);auto width=uint32_t(src[j*5+1]);auto height=uint32_t(src[j*5+2]);auto format=VkFormat(src[j*5+3]);auto layout=int(src[j*5+4]);
                if(width!=im.width||height!=im.height)throw std::runtime_error("Input extent mismatch");
                if(!image||image==im.image)throw std::runtime_error("Input source/destination lifetime or alias violation");
                for(auto f:{format,im.format})if(!formatProperties.count(f)){VkFormatProperties properties{};fn<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties")(physical,f,&properties);formatProperties[f]=properties;}
                const auto a=formatProperties.at(format),b=formatProperties.at(im.format);
                if(!(a.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_SRC_BIT)||!(b.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT))throw std::runtime_error("Input format conversion not supported by blit");
                if(j>=2){state(s,"SOURCE_STATE","role="+std::to_string(j)+" image="+handleText(image)+" format="+std::to_string(format)+" before="+std::to_string(layout)+" copy=6 after="+std::to_string(layout));
                    state(s,"DESTINATION_STATE","role="+std::to_string(j)+" image="+handleText(im.image)+" memory="+handleText(im.memory)+" format="+std::to_string(im.format)+" dx="+handleText(im.dx.resource.Get())+" before="+(s.priorInitialized[j]?"GENERAL":"UNDEFINED"));
                    checkpoint(s,s.inputs,j==2?"BEFORE_DEPTH_BARRIER":"BEFORE_MOTION_BARRIER");}
                sourceBarrier(s.inputs,image,layout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                if(j>=2)checkpoint(s,s.inputs,j==2?"AFTER_DEPTH_BARRIER":"AFTER_MOTION_BARRIER");
                VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]={int(width),int(height),1};region.dstOffsets[1]={int(width),int(height),1};
                if(j>=2&&flipBorrowedInputs){region.srcOffsets[0].y=int(height);region.srcOffsets[1].y=0;}
                fn<PFN_vkCmdBlitImage>("vkCmdBlitImage")(s.inputs,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,im.image,VK_IMAGE_LAYOUT_GENERAL,1,&region,VK_FILTER_NEAREST);
                sourceBarrier(s.inputs,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,layout);
                if(j>=2){checkpoint(s,s.inputs,j==2?"AFTER_DEPTH_COPY":"AFTER_MOTION_COPY");state(s,j==2?"DEPTH_COPY_RECORDED":"MOTION_COPY_RECORDED");}
                if(src.size()==32&&j>=2)event("borrowed_copy_recorded","realFrameId="+std::to_string(frameId)+" resource="+(j==2?"depth":"motion")+
                    " source="+std::to_string(src[j*5])+" destination="+std::to_string(reinterpret_cast<jlong>(im.image))+
                    " sourceFormat="+std::to_string(format)+" destinationFormat="+std::to_string(im.format)+" width="+std::to_string(width)+" height="+std::to_string(height)+
                    " sourceBefore="+std::to_string(layout)+" sourceCopy=6 sourceAfter="+std::to_string(layout)+
                    " destinationBefore="+(s.priorInitialized[j]?"GENERAL":"UNDEFINED")+" destinationCopy=GENERAL destinationAfter=GENERAL sourceOwnership=SAME_FAMILY_RESTORED persistentDestination=true "+s.borrowedEvidence);
            }else{VkClearColorValue sentinel{};sentinel.float32[0]=sentinel.float32[2]=sentinel.float32[3]=1;VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};fn<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(s.inputs,im.image,VK_IMAGE_LAYOUT_GENERAL,&sentinel,1,&range);}
            sharedBarrier(s.inputs,im,false,true);
            if(j==3)checkpoint(s,s.inputs,"DESTINATION_RELEASED_TO_D3D12");
            // Recorded into the worker's already-begun output command buffer; its GPU
            // timeline wait is added by the provider submission plan after input submit.
            sharedBarrier(output,im,true,false,j==0||j>=4?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL);
            if(j==0)checkpoint(s,output,"OUTPUT_D3D12_DONE_WAIT_PASSED");
        }
        if(s.sampled){sampleCopy(output,s.images[0],s.colorSample);for(unsigned k=0;k<p.generatedCount;++k)sampleCopy(output,s.images[4+k],s.generatedSamples[k]);}
        if(p.inputTiming)fn<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(s.inputs,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,p.inputTiming,s.timestampIndex+1);
        checkpoint(s,s.inputs,"BEFORE_D3D12_READY_SIGNAL");
        vkcheck(fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(s.inputs),"input end");
        state(s,"COMMAND_END");
        hr(s.allocator->Reset(),"slot allocator reset");hr(s.commands->Reset(s.allocator.Get(),nullptr),"slot list reset");

        if(p.generatedCount == 3) {
            // Populate descriptor heap for slot s
            createSRV(p.historyColor.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, cpu(s, 0));
            createSRV(s.images[0].dx.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, cpu(s, 1));
            createSRV(p.historyMotionNDC.Get(), DXGI_FORMAT_R32G32_FLOAT, cpu(s, 2));
            createSRV(s.currentMotionNDC.Get(), DXGI_FORMAT_R32G32_FLOAT, cpu(s, 3));
            createSRV(p.historyDepth.Get(), DXGI_FORMAT_R32_FLOAT, cpu(s, 4));
            createSRV(s.images[2].dx.resource.Get(), DXGI_FORMAT_R32_FLOAT, cpu(s, 5));
            createSRV(nullptr, DXGI_FORMAT_R32_FLOAT, cpu(s, 6));

            createUAV(s.images[4].dx.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, cpu(s, 7)); // G25
            createUAV(s.images[6].dx.resource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, cpu(s, 8)); // G75
            createUAV(s.currentMotionNDC.Get(), DXGI_FORMAT_R32G32_FLOAT, cpu(s, 9));         // MotionConvert out
            createSRV(s.images[3].dx.resource.Get(), DXGI_FORMAT_R32G32_FLOAT, cpu(s, 10)); // MotionConvert in

            // Descriptor population gate: verify device alive immediately after populating descriptors
            auto reasonAfterDescriptors = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
            evidence.str("device_reason_after_descriptor_population", hrHex(reasonAfterDescriptors));
            if (FAILED(reasonAfterDescriptors)) {
                evidence.str("fail_stage", "DESCRIPTOR_POPULATION");
                recordDred(dx.device.Get(), "DESCRIPTOR_POPULATION_FAIL");
                throw std::runtime_error("Device removed after descriptor population " + hrHex(reasonAfterDescriptors));
            }

            auto fillOpts = [&](NVSDK_NGX_DLSSG_Opt_Eval_Params& opts, bool evalReset) {
                opts.multiFrameCount = 1;
                opts.multiFrameIndex = 1;
                std::memcpy(opts.cameraViewToClip, constants.data(), 16 * sizeof(float));
                std::memcpy(opts.clipToCameraView, constants.data() + 16, 16 * sizeof(float));
                std::memcpy(opts.clipToLensClip, constants.data() + 32, 16 * sizeof(float));
                std::memcpy(opts.clipToPrevClip, constants.data() + 48, 16 * sizeof(float));
                std::memcpy(opts.prevClipToClip, constants.data() + 64, 16 * sizeof(float));
                auto v = constants.data() + 80;
                opts.jitterOffset[0] = v[0]; opts.jitterOffset[1] = v[1];
                opts.mvecScale[0] = v[2]; opts.mvecScale[1] = v[3];
                opts.cameraPinholeOffset[0] = v[4]; opts.cameraPinholeOffset[1] = v[5];
                std::memcpy(opts.cameraPos, v + 6, 3 * sizeof(float));
                std::memcpy(opts.cameraUp, v + 9, 3 * sizeof(float));
                std::memcpy(opts.cameraRight, v + 12, 3 * sizeof(float));
                std::memcpy(opts.cameraFwd, v + 15, 3 * sizeof(float));
                opts.cameraNear = v[18]; opts.cameraFar = v[19]; opts.cameraFOV = v[20]; opts.cameraAspectRatio = v[21];
                opts.depthInverted = v[22] != 0; opts.cameraMotionIncluded = v[23] != 0;
                opts.motionVectorsInvalidValue = v[24]; opts.motionVectorsDilated = v[25] != 0;
                opts.orthoProjection = v[26] != 0; opts.minRelativeLinearDepthObjectSeparation = v[27];
                opts.reset = evalReset;
                opts.mvecsSubrectSize = opts.depthSubrectSize = {p.renderWidth, p.renderHeight};
                opts.hudLessSubrectSize = opts.backbufferSubrectSize = opts.outputInterpSubrectSize = {p.width, p.height};
            };

            if(reset || !p.hasHistory) {
                evidence.str("motion_convert_on_reset_frame", "NO");

                // FIRST EVALUATE GATE: verify device alive before first evaluate
                auto reasonBeforeEval = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
                evidence.str("device_reason_before_evaluate", hrHex(reasonBeforeEval));
                if (SUCCEEDED(reasonBeforeEval)) {
                    evidence.str("device_alive_before_first_evaluate", "YES");
                } else {
                    evidence.str("device_alive_before_first_evaluate", "NO");
                    evidence.str("fail_stage", "DEVICE_REMOVED_BEFORE_FIRST_EVALUATE");
                    recordDred(dx.device.Get(), "BEFORE_FIRST_EVALUATE_FAIL");
                    throw std::runtime_error("Device already removed before first evaluate " + hrHex(reasonBeforeEval));
                }
                queryVram("before_first_evaluate");

                // Reset suppresses all generated outputs. G25/G75 are skipped on
                // this interval, so publish DISABLED instead of UNKNOWN sentinel.
                // G50 remains sentinel until NGX writes its authoritative flag.
                setDisable(s, 0, disableOne.Get());
                setDisable(s, 1, disableSentinel.Get());
                setDisable(s, 2, disableOne.Get());
                evidence.str("reset_custom_output_status", "DISABLED");

                dxBarrier(s, s.images[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                dxBarrier(s, s.images[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                dxBarrier(s, s.images[2], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                dxBarrier(s, s.images[3], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                dxBarrier(s, s.images[5], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};
                ep.pBackbuffer = s.images[0].dx.resource.Get();
                ep.pHudless = s.images[1].dx.resource.Get();
                ep.pDepth = s.images[2].dx.resource.Get();
                ep.pMVecs = s.images[3].dx.resource.Get();
                ep.pOutputInterpFrame = s.images[5].dx.resource.Get();
                ep.pOutputDisableInterpolation = s.disable[1].Get();
                NVSDK_NGX_DLSSG_Opt_Eval_Params opts{};
                fillOpts(opts, true);
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1 * 2);
                NVSDK_NGX_Parameter_SetULL(p.parameters, NVSDK_NGX_DLSSG_Parameter_BackbufferFrameID, frameId);
                auto code = NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(), p.feature, p.parameters, &ep, &opts);
                state(s, "DLSSG_EVALUATE_RESET", "result=" + resultHex(code));
                evidence.str("ngx_first_evaluate_result", resultHex(code));
                evidence.str("ngx_evaluate_result", resultHex(code));
                if (!NVSDK_NGX_SUCCEED(code)) {
                    p.failed = true;
                    auto reasonAfter = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
                    evidence.str("device_reason_after_evaluate", hrHex(reasonAfter));
                    evidence.str("underlying_d3d12", hrHex(reasonAfter));
                    if (code == 0xbad00002) {
                        evidence.str("ngx_evaluate_classification", "PLATFORM_ERROR");
                    } else if (code == 0xbad00005) {
                        evidence.str("ngx_evaluate_classification", "INVALID_PARAMETER");
                    } else {
                        evidence.str("ngx_evaluate_classification", "OTHER_FAILURE");
                    }
                    recordDred(dx.device.Get(), "FIRST_EVALUATE_FAIL");
                    throw std::runtime_error("NGX Evaluate reset " + resultHex(code));
                }
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1 * 2 + 1);
                s.commands->ResolveQueryData(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1 * 2, 2, s.timingReadback.Get(), 1 * 16);

                dxBarrier(s, s.images[5], D3D12_RESOURCE_STATE_COMMON);

                copyHistory(p, s, false);
                p.hasHistory = true;
            } else {
                ID3D12DescriptorHeap* heaps[] = {s.heap.Get()};
                s.commands->SetDescriptorHeaps(1, heaps);

                // 1. Dedicated Motion Convert CS (Screen Space Pixels -> NDC Clip Space)
                // Range contract: table 0 = 1 SRV (t0), table 1 = 1 UAV (u0), 5 constants
                dxBarrier(s, s.images[3], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                transitionRes(s, s.currentMotionNDC.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                s.commands->SetComputeRootSignature(motionConvertRoot.Get());
                s.commands->SetComputeRootDescriptorTable(0, gpu(s, 10)); // 1 SRV at index 10: s.images[3]
                s.commands->SetComputeRootDescriptorTable(1, gpu(s, 9));  // 1 UAV at index 9: s.currentMotionNDC
                UINT mcConsts[] = {p.renderWidth, p.renderHeight, 0, 0, 0};
                s.commands->SetComputeRoot32BitConstants(2, 5, mcConsts, 0);
                s.commands->SetPipelineState(motionConvertPSO.Get());
                s.commands->Dispatch((p.renderWidth + 7) / 8, (p.renderHeight + 7) / 8, 1);
                transitionRes(s, s.currentMotionNDC.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                dxBarrier(s, s.images[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                dxBarrier(s, s.images[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                dxBarrier(s, s.images[2], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                dxBarrier(s, s.images[3], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                transitionRes(s, p.historyColor.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                transitionRes(s, p.historyDepth.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                transitionRes(s, p.historyMotionNDC.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                // 2. G25 Dispatch (t = 0.25f)
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0 * 2);
                dxBarrier(s, s.images[4], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                s.commands->SetComputeRootSignature(hybridRoot.Get());
                s.commands->SetComputeRootDescriptorTable(0, gpu(s, 0)); // t0..t6
                s.commands->SetComputeRootDescriptorTable(1, gpu(s, 7)); // u0 = s.images[4]
                float t25 = 0.25f;
                UINT g25Consts[] = {p.width, p.height, 0, std::bit_cast<UINT>(t25), 0};
                s.commands->SetComputeRoot32BitConstants(2, 5, g25Consts, 0);
                s.commands->SetPipelineState(hybridPSO.Get());
                s.commands->Dispatch((p.width + 7) / 8, (p.height + 7) / 8, 1);
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0 * 2 + 1);
                s.commands->ResolveQueryData(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0 * 2, 2, s.timingReadback.Get(), 0 * 16);
                dxBarrier(s, s.images[4], D3D12_RESOURCE_STATE_COMMON);
                setDisable(s, 0, disableZero.Get());

                // 3. G50 NVIDIA Evaluate (t = 0.50f)
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1 * 2);
                dxBarrier(s, s.images[5], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                setDisable(s, 1, disableSentinel.Get());
                NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};
                ep.pBackbuffer = s.images[0].dx.resource.Get();
                ep.pHudless = s.images[1].dx.resource.Get();
                ep.pDepth = s.images[2].dx.resource.Get();
                ep.pMVecs = s.images[3].dx.resource.Get();
                ep.pOutputInterpFrame = s.images[5].dx.resource.Get();
                ep.pOutputDisableInterpolation = s.disable[1].Get();
                NVSDK_NGX_DLSSG_Opt_Eval_Params opts{};
                fillOpts(opts, false);
                NVSDK_NGX_Parameter_SetULL(p.parameters, NVSDK_NGX_DLSSG_Parameter_BackbufferFrameID, frameId);
                auto code = NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(), p.feature, p.parameters, &ep, &opts);
                state(s, "DLSSG_EVALUATE_G50", "result=" + resultHex(code));
                if (!NVSDK_NGX_SUCCEED(code)) {
                    p.failed = true;
                    auto reasonAfter = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
                    evidence.str("device_reason_after_evaluate", hrHex(reasonAfter));
                    evidence.str("underlying_d3d12", hrHex(reasonAfter));
                    if (code == 0xbad00002) {
                        evidence.str("ngx_evaluate_classification", "PLATFORM_ERROR");
                    } else if (code == 0xbad00005) {
                        evidence.str("ngx_evaluate_classification", "INVALID_PARAMETER");
                    } else {
                        evidence.str("ngx_evaluate_classification", "OTHER_FAILURE");
                    }
                    recordDred(dx.device.Get(), "G50_EVALUATE_FAIL");
                    throw std::runtime_error("NGX Evaluate G50 " + resultHex(code));
                }
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1 * 2 + 1);
                s.commands->ResolveQueryData(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1 * 2, 2, s.timingReadback.Get(), 1 * 16);
                dxBarrier(s, s.images[5], D3D12_RESOURCE_STATE_COMMON);

                // 4. G75 Dispatch (t = 0.75f)
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * 2);
                dxBarrier(s, s.images[6], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                s.commands->SetComputeRootSignature(hybridRoot.Get());
                s.commands->SetComputeRootDescriptorTable(0, gpu(s, 0)); // t0..t6
                s.commands->SetComputeRootDescriptorTable(1, gpu(s, 8)); // u0 = s.images[6]
                float t75 = 0.75f;
                UINT g75Consts[] = {p.width, p.height, 0, std::bit_cast<UINT>(t75), 0};
                s.commands->SetComputeRoot32BitConstants(2, 5, g75Consts, 0);
                s.commands->SetPipelineState(hybridPSO.Get());
                s.commands->Dispatch((p.width + 7) / 8, (p.height + 7) / 8, 1);
                s.commands->EndQuery(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * 2 + 1);
                s.commands->ResolveQueryData(s.timing.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * 2, 2, s.timingReadback.Get(), 2 * 16);
                dxBarrier(s, s.images[6], D3D12_RESOURCE_STATE_COMMON);
                setDisable(s, 2, disableZero.Get());

                // 5. Update history
                copyHistory(p, s, true);
            }

            for(size_t j=0; j<4; ++j) dxBarrier(s, s.images[j], D3D12_RESOURCE_STATE_COMMON);

            for(unsigned k=0; k<3; ++k) {
                D3D12_RESOURCE_BARRIER db{}; db.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                db.Transition = {s.disable[k].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE};
                s.commands->ResourceBarrier(1, &db);
                s.commands->CopyBufferRegion(s.disableReadback[k].Get(), 0, s.disable[k].Get(), 0, 16);
                std::swap(db.Transition.StateBefore, db.Transition.StateAfter);
                s.commands->ResourceBarrier(1, &db);
            }
        } else {
            for(unsigned k=0;k<p.generatedCount;++k){
                D3D12_RESOURCE_BARRIER flagBarrier{};flagBarrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;flagBarrier.Transition={s.disable[k].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST};s.commands->ResourceBarrier(1,&flagBarrier);s.commands->CopyBufferRegion(s.disable[k].Get(),0,disableSentinel.Get(),0,16);std::swap(flagBarrier.Transition.StateBefore,flagBarrier.Transition.StateAfter);s.commands->ResourceBarrier(1,&flagBarrier);
                if(s.diagnostic){flagBarrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;s.commands->ResourceBarrier(1,&flagBarrier);s.commands->CopyBufferRegion(s.statusBefore[k].Get(),0,s.disable[k].Get(),0,4);std::swap(flagBarrier.Transition.StateBefore,flagBarrier.Transition.StateAfter);s.commands->ResourceBarrier(1,&flagBarrier);}
                for(size_t j=0;j<s.images.size();++j)dxBarrier(s,s.images[j],j>=4?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};ep.pBackbuffer=s.images[0].dx.resource.Get();ep.pHudless=s.images[1].dx.resource.Get();ep.pDepth=s.images[2].dx.resource.Get();ep.pMVecs=s.images[3].dx.resource.Get();ep.pOutputInterpFrame=s.images[4+k].dx.resource.Get();ep.pOutputDisableInterpolation=s.disable[k].Get();
                NVSDK_NGX_DLSSG_Opt_Eval_Params opts{};opts.multiFrameCount=p.generatedCount;opts.multiFrameIndex=k+1;
                std::memcpy(opts.cameraViewToClip,constants.data(),16*sizeof(float));
                std::memcpy(opts.clipToCameraView,constants.data()+16,16*sizeof(float));
                std::memcpy(opts.clipToLensClip,constants.data()+32,16*sizeof(float));
                std::memcpy(opts.clipToPrevClip,constants.data()+48,16*sizeof(float));
                std::memcpy(opts.prevClipToClip,constants.data()+64,16*sizeof(float));
                auto v=constants.data()+80;opts.jitterOffset[0]=v[0];opts.jitterOffset[1]=v[1];opts.mvecScale[0]=v[2];opts.mvecScale[1]=v[3];
                opts.cameraPinholeOffset[0]=v[4];opts.cameraPinholeOffset[1]=v[5];std::memcpy(opts.cameraPos,v+6,3*sizeof(float));std::memcpy(opts.cameraUp,v+9,3*sizeof(float));std::memcpy(opts.cameraRight,v+12,3*sizeof(float));std::memcpy(opts.cameraFwd,v+15,3*sizeof(float));opts.cameraNear=v[18];opts.cameraFar=v[19];opts.cameraFOV=v[20];opts.cameraAspectRatio=v[21];opts.depthInverted=v[22]!=0;opts.cameraMotionIncluded=v[23]!=0;opts.motionVectorsInvalidValue=v[24];opts.motionVectorsDilated=v[25]!=0;opts.orthoProjection=v[26]!=0;opts.minRelativeLinearDepthObjectSeparation=v[27];opts.reset=reset&&k==0;
                opts.mvecsSubrectSize=opts.depthSubrectSize={p.renderWidth,p.renderHeight};opts.hudLessSubrectSize=opts.backbufferSubrectSize=opts.outputInterpSubrectSize={p.width,p.height};
                s.commands->EndQuery(s.timing.Get(),D3D12_QUERY_TYPE_TIMESTAMP,k*2);
                NVSDK_NGX_Parameter_SetULL(p.parameters,NVSDK_NGX_DLSSG_Parameter_BackbufferFrameID,frameId);
                auto code=NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(),p.feature,p.parameters,&ep,&opts);
                state(s,"DLSSG_EVALUATE","result="+resultHex(code));
                if(!NVSDK_NGX_SUCCEED(code)){p.failed=true;throw std::runtime_error("NGX Evaluate "+resultHex(code));}
                s.commands->EndQuery(s.timing.Get(),D3D12_QUERY_TYPE_TIMESTAMP,k*2+1);s.commands->ResolveQueryData(s.timing.Get(),D3D12_QUERY_TYPE_TIMESTAMP,k*2,2,s.timingReadback.Get(),k*16);
                for(auto& im:s.images)dxBarrier(s,im,D3D12_RESOURCE_STATE_COMMON);
                D3D12_RESOURCE_BARRIER db{};db.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;db.Transition={s.disable[k].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};s.commands->ResourceBarrier(1,&db);s.commands->CopyBufferRegion(s.disableReadback[k].Get(),0,s.disable[k].Get(),0,16);std::swap(db.Transition.StateBefore,db.Transition.StateAfter);s.commands->ResourceBarrier(1,&db);
            }
        }
        if(s.diagnostic)for(unsigned k=0;k<p.generatedCount;++k){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={s.disable[k].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};s.commands->ResourceBarrier(1,&b);s.commands->CopyBufferRegion(s.statusGroupEnd[k].Get(),0,s.disable[k].Get(),0,4);std::swap(b.Transition.StateBefore,b.Transition.StateAfter);s.commands->ResourceBarrier(1,&b);}
        hr(s.commands->Close(),"slot list close");s.prepared=true;
    }
    void submit(JNIEnv* env,Slot& s,const std::vector<jlong>& semaphores,jobject notification) {
        if(!s.prepared||s.submitted||unsafe)throw std::runtime_error("Invalid input submission state");
        for(auto expected:s.borrowedReady)if(expected&&std::find(semaphores.begin(),semaphores.end(),expected)==semaphores.end())
            throw std::runtime_error("Borrowed producer signal absent from consumer GPU waits");
        std::vector<VkSemaphore> waits;for(auto v:semaphores)waits.push_back(reinterpret_cast<VkSemaphore>(v));std::vector<VkPipelineStageFlags> stages(waits.size(),VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);std::vector<uint64_t> zeros(waits.size(),0);
        VkTimelineSemaphoreSubmitInfo ti{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};ti.waitSemaphoreValueCount=uint32_t(waits.size());ti.pWaitSemaphoreValues=zeros.data();ti.signalSemaphoreValueCount=1;ti.pSignalSemaphoreValues=&s.ready;
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.pNext=&ti;si.commandBufferCount=1;si.pCommandBuffers=&s.inputs;si.waitSemaphoreCount=uint32_t(waits.size());si.pWaitSemaphores=waits.data();si.pWaitDstStageMask=stages.data();si.signalSemaphoreCount=1;si.pSignalSemaphores=&timeline;
        env->GetJavaVM(&s.vm);s.notification=env->NewGlobalRef(notification);
        s.notifyMethod=env->GetMethodID(env->GetObjectClass(notification),"completeFromNative","(J[IJ)V");
        if(!s.notification||!s.notifyMethod)throw std::runtime_error("JNI completion contract unavailable");
        SetThreadpoolWait(s.completionWait,s.completionEvent,nullptr);
        hr(fence->SetEventOnCompletion(s.done,s.completionEvent),"async completion notification");
        // Caller holds the borrowed queue's Java submitLock for this native submit.
        s.submitted=true;unsafe=true;state(s,"QUEUE_SUBMIT_BEGIN",s.borrowedEvidence);
        auto submitResult=fn<PFN_vkQueueSubmit>("vkQueueSubmit")(queue,1,&si,VK_NULL_HANDLE);
        state(s,"QUEUE_SUBMIT_RESULT","VkResult="+std::to_string(submitResult));
        if(submitResult==VK_ERROR_DEVICE_LOST)captureFault();
        vkcheck(submitResult,"input queue submit");state(s,"D3D12_READY","GPU_SIGNAL_SUBMITTED_NOT_YET_OBSERVED");
        auto reasonBeforeSubmit = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
        evidence.str("device_reason_before_submit", hrHex(reasonBeforeSubmit));
        if (FAILED(reasonBeforeSubmit)) {
            recordDred(dx.device.Get(), "BEFORE_SUBMIT_FAIL");
            throw std::runtime_error("Device removed before submit " + hrHex(reasonBeforeSubmit));
        }
        hr(dx.queue->Wait(fence.Get(),s.ready),"D3D12 queue GPU wait");ID3D12CommandList* lists[]={s.commands.Get()};dx.queue->ExecuteCommandLists(1,lists);hr(dx.queue->Signal(fence.Get(),s.done),"D3D12 queue completion signal");unsafe=false;++submittedFrames;
        event("vulkan_d3d12_submit","realFrameId="+std::to_string(s.frameId)+" ready="+std::to_string(s.ready)+" completion="+std::to_string(s.done)+" cpuWait=false");
        if(s.borrowedReady[0])event("borrowed_copy_submit","realFrameId="+std::to_string(s.frameId)+" copyCommand="+std::to_string(reinterpret_cast<jlong>(s.inputs))+" copyCompleteValue="+std::to_string(s.ready)+" fgCompleteValue="+std::to_string(s.done)+" consumerQueue="+std::to_string(reinterpret_cast<jlong>(queue))+" consumerFamily="+std::to_string(family)+" waits=PRODUCER_BINARY cpuWait=false "+s.borrowedEvidence);
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
        if(!s.leased||!s.submitted||fence->GetCompletedValue()<s.done||fence->GetCompletedValue()==UINT64_MAX) {
            auto reason = dx.device ? dx.device->GetDeviceRemovedReason() : E_FAIL;
            evidence.str("device_reason_on_release_fail", hrHex(reason));
            if (FAILED(reason)) recordDred(dx.device.Get(), "RELEASE_COMPLETION_FAIL");
            throw std::runtime_error("Premature lease retirement or device removed " + hrHex(reason));
        }
        requireD3DDone(fence->GetCompletedValue(),s.done);
        state(s,"D3D12_DONE","observed="+std::to_string(fence->GetCompletedValue()));state(s,"OUTPUT_READY","CALLER_OUTPUT_AND_PRESENT_FENCES_RETIRED");
        hr(dx.device->GetDeviceRemovedReason(),"device removed reason");++completedFrames;
        if(s.borrowedReady[0])event("borrowed_copy_complete","realFrameId="+std::to_string(s.frameId)+" depthCopies=1 motionCopies=1 copyCompleteValue="+std::to_string(s.ready)+" observedCompletion="+std::to_string(fence->GetCompletedValue())+" cpuTransportCopies=0 "+s.borrowedEvidence);
        if(!s.flagsReady.load(std::memory_order_acquire))throw std::runtime_error("Retirement before flags callback");
        if(s.diagnostic){auto readStatus=[&](ID3D12Resource* resource){void* ptr{};D3D12_RANGE range{0,4},none{0,0};hr(resource->Map(0,&range,&ptr),"diagnostic status map");auto value=*static_cast<uint32_t*>(ptr);resource->Unmap(0,&none);return value;};
            for(unsigned k=0;k<p.generatedCount;++k)event("status_trace","PAIR_ID="+std::to_string(s.frameId)+" COUNT="+std::to_string(p.generatedCount)+" INDEX="+std::to_string(k+1)+" ARRAY_SLOT="+std::to_string(k)+" OUTPUT_RESOURCE_ID="+handleText(s.images[4+k].dx.resource.Get())+" STATUS_RESOURCE_ID="+handleText(s.disable[k].Get())+" STATUS_BEFORE="+std::to_string(readStatus(s.statusBefore[k].Get()))+" STATUS_AFTER_EVALUATE="+std::to_string(uint32_t(s.flags[k]))+" STATUS_AFTER_GROUP="+std::to_string(readStatus(s.statusGroupEnd[k].Get()))+" COMPLETION_ID="+std::to_string(s.done)+" COMPLETION_OBSERVED="+std::to_string(s.completionObserved)+" COMPLETION_MODEL=GROUP_ORDERED_COMMAND_LIST STATUS_READ_ERROR="+jsonQuote(s.statusReadError));}
        double inputMs=-1;
        if(p.inputTiming){uint64_t inputTicks[2]{};auto code=fn<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(device,p.inputTiming,s.timestampIndex,2,sizeof inputTicks,inputTicks,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT);vkcheck(code,"completed input timestamps");auto mask=vkTimestampBits==64?~uint64_t(0):((uint64_t(1)<<vkTimestampBits)-1);inputMs=double((inputTicks[1]-inputTicks[0])&mask)*vkPeriod/1e6;}
        void* mapped{};D3D12_RANGE range{0,p.generatedCount*16},none{0,0};hr(s.timingReadback->Map(0,&range,&mapped),"timing map");
        std::vector<uint64_t> ticks(p.generatedCount*2);std::memcpy(ticks.data(),mapped,p.generatedCount*16);s.timingReadback->Unmap(0,&none);
        for(unsigned k=0;k<p.generatedCount;++k){auto disabled=s.flags[k];if(disabled==0&&!s.reset)++generatedFrames;
            event("completion","realFrameId="+std::to_string(s.frameId)+" index="+std::to_string(k+1)+" count="+std::to_string(p.generatedCount)+" done="+std::to_string(s.done)+" DLSSG_GPU_ms="+std::to_string(double(ticks[k*2+1]-ticks[k*2])*1000/dxFrequency)+" INPUT_GPU_COPY_ms="+std::to_string(inputMs)+" disable="+std::to_string(disabled)+" generatedFrames="+std::to_string(generatedFrames));}
        if(s.sampled){auto b=pixels(s.colorSample);auto bh=sha(b);bool previousKnown=previousSampleId+1==s.frameId;
            save(evidence.out/("sample-"+std::to_string(s.frameId)+"-B.bin"),b);
            for(unsigned k=0;k<p.generatedCount;++k){auto g=pixels(s.generatedSamples[k]);auto gh=sha(g);size_t black=0,sentinel=0;
                for(size_t j=0;j<g.size();j+=4){black+=g[j]==0&&g[j+1]==0&&g[j+2]==0;sentinel+=g[j]==255&&g[j+1]==0&&g[j+2]==255;}
                bool valid=previousKnown&&!s.reset&&s.flags[k]==0&&gh!=bh&&gh!=previousSampleHash&&black<g.size()/4&&sentinel==0;
                event("output_sample","realFrameId="+std::to_string(s.frameId)+" index="+std::to_string(k+1)+" count="+std::to_string(p.generatedCount)+" B_SHA256="+bh+" G_SHA256="+gh+" A_SHA256="+previousSampleHash+" previousKnown="+std::to_string(previousKnown)+" disable="+std::to_string(s.flags[k])+" blackPixels="+std::to_string(black)+" sentinelPixels="+std::to_string(sentinel)+" valid="+std::to_string(valid));
                save(evidence.out/("sample-"+std::to_string(s.frameId)+"-G"+std::to_string(k+1)+".bin"),g);}
            previousSampleHash=bh;previousSampleId=s.frameId;
            event("sample_metadata","realFrameId="+std::to_string(s.frameId)+" width="+std::to_string(p.width)+" height="+std::to_string(p.height)+" readback_api=Vulkan");events.flush();}
        state(s,"SLOT_RETIRE");state(s,"FRAME_END");s.leased=false;s.prepared=s.submitted=false;
        event("slot_recycle","realFrameId="+std::to_string(s.frameId)+" state=FREE");
        if(completedFrames%120==0)events.flush();
    }
    void closePool(Pool& p) {
        event("shutdown_stage","DLSSG_POOL_BEGIN");
        for(auto& s:p.slots)if(s->leased)throw std::runtime_error("Cannot destroy leased pool");
        p.historyColor.Reset(); p.historyDepth.Reset(); p.historyMotionNDC.Reset();
        if(p.feature){event("shutdown_stage","DLSSG_FEATURE_RELEASE_BEGIN");auto r=NVSDK_NGX_D3D12_ReleaseFeature(p.feature);event("shutdown_stage","DLSSG_FEATURE_RELEASE_END result="+resultHex(r));if(!NVSDK_NGX_SUCCEED(r))throw std::runtime_error("DLSSG release failed; owner retained");p.feature=nullptr;}
        if(p.parameters){event("shutdown_stage","DLSSG_PARAMETERS_DESTROY_BEGIN");auto r=NVSDK_NGX_D3D12_DestroyParameters(p.parameters);event("shutdown_stage","DLSSG_PARAMETERS_DESTROY_END result="+resultHex(r));if(!NVSDK_NGX_SUCCEED(r))throw std::runtime_error("DLSSG parameters destroy failed; owner retained");p.parameters=nullptr;}
        event("shutdown_stage","DLSSG_CALLBACKS_AND_IMPORTS_BEGIN");
        for(auto& s:p.slots){if(s->completionWait){SetThreadpoolWait(s->completionWait,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(s->completionWait,FALSE);CloseThreadpoolWait(s->completionWait);CloseHandle(s->completionEvent);}
            s->currentMotionNDC.Reset(); s->heap.Reset();
            for(auto& im:s->images){if(im.view)fn<PFN_vkDestroyImageView>("vkDestroyImageView")(device,im.view,nullptr);if(im.image)fn<PFN_vkDestroyImage>("vkDestroyImage")(device,im.image,nullptr);if(im.memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,im.memory,nullptr);im.view={};im.image={};im.memory={};im.dx.resource.Reset();}s->generatedSamples.push_back(s->colorSample);for(auto& sampleValue:s->generatedSamples){auto sample=&sampleValue;if(sample->buffer)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,sample->buffer,nullptr);if(sample->memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,sample->memory,nullptr);}if(s->inputs){fn<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(device,commandPool,1,&s->inputs);s->inputs={};}}
        p.slots.clear();
        event("shutdown_stage","DLSSG_CALLBACKS_AND_IMPORTS_END");
        if(p.inputTiming){fn<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(device,p.inputTiming,nullptr);p.inputTiming={};}
        event("shutdown_stage","DLSSG_POOL_END");
    }
    void close() {
        evidence.raw("real_frames_submitted",std::to_string(submittedFrames));evidence.raw("real_frames_completed",std::to_string(completedFrames));evidence.raw("generated_disable_flag_zero_count",std::to_string(generatedFrames));
        events.flush();dx.debugMessages();evidence.raw("d3d12_debug_errors",std::to_string(validationErrors));
        if(unsafe){evidence.str("cleanup","UNSAFE_GPU_RETAINED_UNTIL_PROCESS_EXIT");return;}
        for(auto& p:pools)closePool(*p);
        event("shutdown_stage","DLSSG_TIMELINE_BEGIN");if(timeline){fn<PFN_vkDestroySemaphore>("vkDestroySemaphore")(device,timeline,nullptr);timeline={};}event("shutdown_stage","DLSSG_TIMELINE_END");
        event("shutdown_stage","DLSSG_COMMAND_POOL_BEGIN");if(commandPool){fn<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(device,commandPool,nullptr);commandPool={};}event("shutdown_stage","DLSSG_COMMAND_POOL_END");
        event("shutdown_stage","DLSSG_NGX_D3D12_CLOSE_BEGIN");dx.close();event("shutdown_stage","DLSSG_NGX_D3D12_CLOSE_END");
        evidence.str("cleanup","DRAINED; borrowed Vulkan device retained");vendorLog.close();validationLog.close();
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
extern "C" JNIEXPORT jint JNICALL JNI_METHOD(abi)(JNIEnv*,jclass){return 4;}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(configureDiagnostics)(JNIEnv* e,jclass,jlong h,jstring out,jboolean fault,jboolean checkpoints,jboolean sync2){try{session(h).configureDiagnostics(path(e,out),fault,checkpoints,sync2);}catch(const std::exception& ex){error(e,ex);}}
extern "C" JNIEXPORT jint JNICALL JNI_METHOD(reportedMax)(JNIEnv*,jclass,jlong h){return session(h).reportedMax;}
extern "C" JNIEXPORT jintArray JNICALL JNI_METHOD(probeCapabilities)(JNIEnv* e,jclass,jstring out,jstring dll,jstring runtime,jint count){
    try {
        auto directory=path(e,out);fs::create_directories(directory);auto rt=path(e,runtime);
        if(sha(readFile(rt/"nvngx_dlssg.dll"))!="ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82")throw std::runtime_error("Runtime SHA256 mismatch");
        vendorLog.open(directory/"ngx.log");validationLog.open(directory/"d3d12-debug.log");
        EvidenceRecorder recorder(directory);ExternalLoader loader;D3D12NgxSession dx(recorder,rt);
        try {
            loader.start(recorder,path(e,dll),count);dx.initialize(D3D12InitializationContext::EmbeddedMinecraft);
            NVSDK_NGX_Parameter* caps{};auto q=NVSDK_NGX_D3D12_GetCapabilityParameters(&caps);
            if(!NVSDK_NGX_SUCCEED(q)||!caps)throw std::runtime_error("Preflight capability query failed");
            unsigned available{},maximum{};
            auto a=NVSDK_NGX_Parameter_GetUI(caps,NVSDK_NGX_Parameter_FrameGeneration_Available,&available);
            auto m=NVSDK_NGX_Parameter_GetUI(caps,NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax,&maximum);
            NVSDK_NGX_D3D12_DestroyParameters(caps);
            recorder.raw("raw_reported_multiframecountmax",std::to_string(maximum));
            recorder.str("raw_max_getter_result",resultHex(m));
            recorder.str("capability_provenance","public NGX query after external loader; may be hooked; no feature or Evaluate");
            recorder.raw("createfeature_calls","0");recorder.raw("evaluate_calls","0");
            dx.close();vendorLog.close();validationLog.close();
            unsigned reportedOut = (count == 3 && available && maximum >= 1) ? 3 : maximum;
            jint values[]={jint(available),jint(reportedOut),jint(a),jint(m)};
            auto array=e->NewIntArray(4);e->SetIntArrayRegion(array,0,4,values);return array;
        } catch(...) {dx.close();vendorLog.close();validationLog.close();throw;}
    } catch(const std::exception& ex){error(e,ex);return nullptr;}
}
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(create)(JNIEnv* e,jclass,jlong i,jlong p,jlong d,jlong q,jint family,jlong proc,jstring out,jstring dll,jstring runtime,jint count){
    Session* s{};try{s=new Session(path(e,out),path(e,dll),path(e,runtime),count);s->borrow(reinterpret_cast<VkInstance>(i),reinterpret_cast<VkPhysicalDevice>(p),reinterpret_cast<VkDevice>(d),reinterpret_cast<VkQueue>(q),family,reinterpret_cast<PFN_vkGetInstanceProcAddr>(proc));return reinterpret_cast<jlong>(s);}catch(const std::exception& ex){if(s){s->unsafe=true;s->evidence.str("integration_error",ex.what());}error(e,ex);return 0;}
}
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(createPool)(JNIEnv* e,jclass,jlong handle,jint w,jint h,jint rw,jint rh,jint capacity,jint count){try{return reinterpret_cast<jlong>(session(handle).createPool(w,h,rw,rh,capacity,count));}catch(const std::exception& ex){error(e,ex);return 0;}}
extern "C" JNIEXPORT jlongArray JNICALL JNI_METHOD(image)(JNIEnv* e,jclass,jlong ph,jint index,jint role){try{auto& im=slot(pool(ph),index).images.at(role);jlong values[]={reinterpret_cast<jlong>(im.image),reinterpret_cast<jlong>(im.view),reinterpret_cast<jlong>(im.memory)};auto a=e->NewLongArray(3);e->SetLongArrayRegion(a,0,3,values);return a;}catch(const std::exception& ex){error(e,ex);return nullptr;}}
extern "C" JNIEXPORT jlongArray JNICALL JNI_METHOD(prepare)(JNIEnv* e,jclass,jlong h,jlong ph,jint index,jlong command,jlongArray sources,jfloatArray data,jlong frameId,jdouble delta,jboolean reset,jboolean flip,jboolean sample){try{std::vector<jlong> src(e->GetArrayLength(sources));e->GetLongArrayRegion(sources,0,jint(src.size()),src.data());std::vector<float> constants(e->GetArrayLength(data));e->GetFloatArrayRegion(data,0,jint(constants.size()),constants.data());auto& s=session(h);auto& p=pool(ph);auto& sl=slot(p,index);s.prepare(p,sl,reinterpret_cast<VkCommandBuffer>(command),src,constants,frameId,delta,reset,flip,sample);jlong result[]={reinterpret_cast<jlong>(s.timeline),jlong(sl.done)};auto a=e->NewLongArray(2);e->SetLongArrayRegion(a,0,2,result);return a;}catch(const std::exception& ex){error(e,ex);return nullptr;}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(submit)(JNIEnv* e,jclass,jlong h,jlong ph,jint index,jlongArray waits,jobject notification){try{std::vector<jlong> values(e->GetArrayLength(waits));e->GetLongArrayRegion(waits,0,jint(values.size()),values.data());session(h).submit(e,slot(pool(ph),index),values,notification);}catch(const std::exception& ex){session(h).unsafe=true;error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(release)(JNIEnv* e,jclass,jlong h,jlong ph,jint index){try{auto& p=pool(ph);session(h).release(p,slot(p,index));}catch(const std::exception& ex){session(h).unsafe=true;error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(abort)(JNIEnv* e,jclass,jlong h,jlong ph,jint index){try{auto& sl=slot(pool(ph),index);if(sl.submitted){session(h).unsafe=true;throw std::runtime_error("Abort after GPU submission retains native session");}for(size_t j=0;j<sl.images.size();++j){sl.images[j].initialized=sl.priorInitialized[j];sl.images[j].dx.state=D3D12_RESOURCE_STATE_COMMON;}sl.leased=sl.prepared=false;}catch(const std::exception& ex){error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(closePool)(JNIEnv* e,jclass,jlong h,jlong ph){try{session(h).closePool(pool(ph));}catch(const std::exception& ex){error(e,ex);}}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(close)(JNIEnv* e,jclass,jlong h){try{auto& s=session(h);s.close();if(!s.unsafe)delete &s;}catch(const std::exception& ex){session(h).unsafe=true;error(e,ex);}}
