"""Reuse the validated stock device initialization in the isolated new harness."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = (root / 'experiments/ngx-dlssg-probe/probe.cpp').read_text()
body = source.split('        vulkan = LoadLibraryExW', 1)[1].split('        auto init = NVSDK_NGX_VULKAN_Init_with_ProjectID', 1)[0]
body = '        vulkan = LoadLibraryExW' + body
body = body.replace('quoted(', 'jsonQuote(')
body = body.replace('auto gipa =', 'gipa =').replace('auto gdpa =', 'gdpa =')
body = body.replace('VkPhysicalDevice physical = {};', 'physical = {};').replace('uint32_t queueIndex = 0;', 'queueIndex = 0;')
body = body.replace('app.pApplicationName = "Wisteria isolated NGX stock capability probe";',
                    'app.pApplicationName = "Isolated SM86 Vulkan x2 external harness";')
body = body.replace('const char* projectId = "72e7a53d-cb8a-4d01-bd3c-98f139a246e5";',
                    'const char* projectId = "3e6891d2-09ac-4f54-ae8d-f481c3150d4b";')
# Assert UUID separately at the exact identity-query boundary.
needle = '        fields << ",\\\"gpu\\\":"'
assert needle in body
body = body.replace(needle, '''        if (hexBytes(ids.deviceUUID, VK_UUID_SIZE) != "01895b66d1ca454d88788dd21fdef638" || !ids.deviceLUIDValid)
            throw std::runtime_error("Physical UUID/LUID does not match the identified NVIDIA GPU");
''' + needle)
needle = '        VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };'
body = body.replace(needle, '''        addExt(instNames, instExt, "VK_EXT_debug_utils");
        instPointers.clear(); for(auto& n:instNames) instPointers.push_back(n.c_str());
        uint32_t layerCount=0; auto enumLayers=load<PFN_vkEnumerateInstanceLayerProperties>(gipa,{},"vkEnumerateInstanceLayerProperties");
        checked(enumLayers(&layerCount,nullptr),"layer count"); std::vector<VkLayerProperties> layers(layerCount); checked(enumLayers(&layerCount,layers.data()),"layers");
        const char* validation="VK_LAYER_KHRONOS_validation";
        bool haveValidation=std::any_of(layers.begin(),layers.end(),[&](auto& l){return std::string(l.layerName)==validation;});
        r.raw("validation_layer_enabled",haveValidation?"true":"false");
''' + needle)
body = body.replace('        checked(load<PFN_vkCreateInstance>', '''        if(haveValidation){ici.enabledLayerCount=1;ici.ppEnabledLayerNames=&validation;}
        checked(load<PFN_vkCreateInstance>''')
body = body.replace('        auto enumGpu =', '''        if(std::find(instNames.begin(),instNames.end(),"VK_EXT_debug_utils")!=instNames.end()){
            VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug.pfnUserCallback=debugCallback; checked(fn<PFN_vkCreateDebugUtilsMessengerEXT>("vkCreateDebugUtilsMessengerEXT")(instance,&debug,nullptr,&messenger),"debug messenger");
        }
        auto enumGpu =''')
# Required extension names must not be silently dropped, unlike a capability-only probe.
body = body.replace('        std::vector<const char*> pointers;', '''        if(NVSDK_NGX_SUCCEED(devRequirementResult)) for(uint32_t i=0;i<requiredCount;++i)
            if(std::find(names.begin(),names.end(),required[i].extensionName)==names.end()) throw std::runtime_error("Required device extension absent");
        std::vector<const char*> pointers;''')
body = body.replace('        VkPhysicalDeviceVulkan12Features f12', '''        fields << ",\\\"instance_extensions\\\": [";
        for(size_t i=0;i<instNames.size();++i)fields<<(i?",":"")<<jsonQuote(instNames[i]);fields<<"]";
        timestampBits=queues[queueIndex].timestampValidBits;timestampPeriod=properties.limits.timestampPeriod;
        r.raw("timestamp_valid_bits",std::to_string(timestampBits));r.raw("timestamp_period_ns",std::to_string(timestampPeriod));
        VkPhysicalDeviceVulkan12Features f12''')
tail = r'''
        fn<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(device,queueIndex,0,&queue);
        r.raw("queue_family",std::to_string(queueIndex));r.str("queue",handleText(queue));
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pci.queueFamilyIndex=queueIndex;pci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        checked(fn<PFN_vkCreateCommandPool>("vkCreateCommandPool")(device,&pci,nullptr,&pool),"vkCreateCommandPool");
        VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cai.commandPool=pool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
        checked(fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(device,&cai,&cmd),"vkAllocateCommandBuffers");
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};checked(fn<PFN_vkCreateFence>("vkCreateFence")(device,&fci,nullptr,&fence),"vkCreateFence");
        if(timestampBits){VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=2;checked(fn<PFN_vkCreateQueryPool>("vkCreateQueryPool")(device,&qi,nullptr,&timestamps),"timestamp pool");}
        r.str("if_fail_stage","NGX_INIT");
        auto init=NVSDK_NGX_VULKAN_Init_with_ProjectID(projectId,NVSDK_NGX_ENGINE_TYPE_CUSTOM,"1.0.0",logPath.c_str(),instance,physical,device,gipa,gdpa,&common);
        initialized=NVSDK_NGX_SUCCEED(init);r.str("ngx_init_result",resultHex(init));r.str("ngx_init",initialized?"PASS":"FAIL");r.modules("modules_after_ngx_init");
        if(!initialized)throw std::runtime_error("NGX init failed "+resultHex(init));
        NVSDK_NGX_Parameter* caps=nullptr;auto cap=NVSDK_NGX_VULKAN_GetCapabilityParameters(&caps);r.str("capability_query_result",resultHex(cap));
        if(NVSDK_NGX_SUCCEED(cap)&&caps){
            for(auto entry:std::vector<std::pair<std::string,const char*>>{{"available",NVSDK_NGX_Parameter_FrameGeneration_Available},{"needs_updated_driver",NVSDK_NGX_Parameter_FrameGeneration_NeedsUpdatedDriver},{"max",NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax}}){
                unsigned int value=0;auto code=NVSDK_NGX_Parameter_GetUI(caps,entry.second,&value);r.str("adapted_reported_"+entry.first+"_getter_result",resultHex(code));r.raw("adapted_reported_"+entry.first,NVSDK_NGX_SUCCEED(code)?std::to_string(value):"null");
            }
            int value=0;auto code=NVSDK_NGX_Parameter_GetI(caps,NVSDK_NGX_Parameter_FrameGeneration_FeatureInitResult,&value);r.str("adapted_reported_feature_init_result_getter_result",resultHex(code));r.raw("adapted_reported_feature_init_result",NVSDK_NGX_SUCCEED(code)?std::to_string(uint32_t(value)):"null");
            NVSDK_NGX_VULKAN_DestroyParameters(caps);
        }
        r.flush();
}
'''
target = root / 'experiments/dlssg-external-harness/session_setup.inc'
target.write_text('''void VulkanNgxSession::initialize(){
        auto& fields=r.bootstrap;
        r.str("if_fail_stage","PRECONDITION");
'''+body+tail)
print('Prepared isolated Vulkan setup from validated stock initialization')
