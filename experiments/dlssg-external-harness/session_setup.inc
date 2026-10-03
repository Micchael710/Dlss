void VulkanNgxSession::initialize(){
        auto& fields=r.bootstrap;
        r.str("if_fail_stage","PRECONDITION");
        vulkan = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!vulkan) throw std::runtime_error("System Vulkan loader unavailable");
        gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkan, "vkGetInstanceProcAddr"));
        if (!gipa) throw std::runtime_error("vkGetInstanceProcAddr unavailable");
        const std::wstring runtimePath = runtime.wstring(), logPath = output.wstring();
        const wchar_t* runtimePaths[] = { runtimePath.c_str() };
        NVSDK_NGX_FeatureCommonInfo common{};
        common.PathListInfo = { runtimePaths, 1 };
        common.LoggingInfo = { ngxLog, NVSDK_NGX_LOGGING_LEVEL_VERBOSE, true };
        // Identifier belongs to this standalone CUSTOM probe; no NVIDIA sample/game AppID.
        const char* projectId = "3e6891d2-09ac-4f54-ae8d-f481c3150d4b";
        NVSDK_NGX_FeatureDiscoveryInfo discovery{};
        discovery.SDKVersion = NVSDK_NGX_Version_API;
        discovery.FeatureID = NVSDK_NGX_Feature_FrameGeneration;
        discovery.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
        discovery.Identifier.v.ProjectDesc = { projectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0.0" };
        discovery.ApplicationDataPath = logPath.c_str();
        discovery.FeatureInfo = &common;
        uint32_t requiredCount = 0;
        VkExtensionProperties* required = nullptr;
        auto instRequirementResult = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&discovery, &requiredCount, &required);
        fields << ",\"instance_extension_query\":" << jsonQuote(resultHex(instRequirementResult));
        auto enumInstExt = load<PFN_vkEnumerateInstanceExtensionProperties>(gipa, {}, "vkEnumerateInstanceExtensionProperties");
        uint32_t count = 0; checked(enumInstExt(nullptr, &count, nullptr), "instance extension count");
        std::vector<VkExtensionProperties> instExt(count); checked(enumInstExt(nullptr, &count, instExt.data()), "instance extensions");
        std::vector<std::string> instNames;
        auto addExt = [](auto& names, const auto& available, const char* name) {
            if (std::any_of(available.begin(), available.end(), [name](const auto& e) { return std::string(e.extensionName) == name; }) &&
                std::find(names.begin(), names.end(), name) == names.end()) names.emplace_back(name);
        };
        if (NVSDK_NGX_SUCCEED(instRequirementResult))
            for (uint32_t i = 0; i < requiredCount; ++i) addExt(instNames, instExt, required[i].extensionName);
        addExt(instNames, instExt, "VK_KHR_get_physical_device_properties2");
        addExt(instNames, instExt, "VK_KHR_external_memory_capabilities");
        addExt(instNames, instExt, "VK_KHR_external_semaphore_capabilities");
        std::vector<const char*> instPointers;
        for (auto& name : instNames) instPointers.push_back(name.c_str());
        VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        app.pApplicationName = "Isolated SM86 Vulkan x2 external harness"; app.apiVersion = VK_API_VERSION_1_2;
        addExt(instNames, instExt, "VK_EXT_debug_utils");
        instPointers.clear(); for(auto& n:instNames) instPointers.push_back(n.c_str());
        uint32_t layerCount=0; auto enumLayers=load<PFN_vkEnumerateInstanceLayerProperties>(gipa,{},"vkEnumerateInstanceLayerProperties");
        checked(enumLayers(&layerCount,nullptr),"layer count"); std::vector<VkLayerProperties> layers(layerCount); checked(enumLayers(&layerCount,layers.data()),"layers");
        const char* validation="VK_LAYER_KHRONOS_validation";
        bool haveValidation=std::any_of(layers.begin(),layers.end(),[&](auto& l){return std::string(l.layerName)==validation;});
        r.raw("validation_layer_enabled",haveValidation?"true":"false");
        VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        ici.pApplicationInfo = &app; ici.enabledExtensionCount = uint32_t(instPointers.size()); ici.ppEnabledExtensionNames = instPointers.data();
        if(haveValidation){ici.enabledLayerCount=1;ici.ppEnabledLayerNames=&validation;}
        checked(load<PFN_vkCreateInstance>(gipa, {}, "vkCreateInstance")(&ici, nullptr, &instance), "vkCreateInstance");
        destroyInstance = load<PFN_vkDestroyInstance>(gipa, instance, "vkDestroyInstance");
        if(std::find(instNames.begin(),instNames.end(),"VK_EXT_debug_utils")!=instNames.end()){
            VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug.pfnUserCallback=debugCallback; checked(fn<PFN_vkCreateDebugUtilsMessengerEXT>("vkCreateDebugUtilsMessengerEXT")(instance,&debug,nullptr,&messenger),"debug messenger");
        }
        auto enumGpu = load<PFN_vkEnumeratePhysicalDevices>(gipa, instance, "vkEnumeratePhysicalDevices");
        checked(enumGpu(instance, &count, nullptr), "physical device count");
        std::vector<VkPhysicalDevice> gpus(count); checked(enumGpu(instance, &count, gpus.data()), "physical devices");
        physical = {};
        VkPhysicalDeviceProperties properties{};
        auto getProperties = load<PFN_vkGetPhysicalDeviceProperties>(gipa, instance, "vkGetPhysicalDeviceProperties");
        for (auto gpu : gpus) {
            VkPhysicalDeviceProperties p{}; getProperties(gpu, &p);
            if (p.vendorID == 0x10de && std::string(p.deviceName).find("3050 Ti") != std::string::npos) { physical = gpu; properties = p; break; }
        }
        if (!physical) throw std::runtime_error("RTX 3050 Ti not found; refusing another GPU");
        VkPhysicalDeviceIDProperties ids{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
        VkPhysicalDeviceDriverProperties driver{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
        ids.pNext = &driver;
        VkPhysicalDeviceProperties2 props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 }; props.pNext = &ids;
        load<PFN_vkGetPhysicalDeviceProperties2>(gipa, instance, "vkGetPhysicalDeviceProperties2")(physical, &props);
        if (hexBytes(ids.deviceUUID, VK_UUID_SIZE) != "01895b66d1ca454d88788dd21fdef638" || !ids.deviceLUIDValid)
            throw std::runtime_error("Physical UUID/LUID does not match the identified NVIDIA GPU");
        fields << ",\"gpu\":" << jsonQuote(properties.deviceName) << ",\"device_uuid\":" << jsonQuote(hexBytes(ids.deviceUUID, VK_UUID_SIZE))
               << ",\"device_luid_valid\":" << (ids.deviceLUIDValid ? "true" : "false")
               << ",\"device_luid\":" << jsonQuote(hexBytes(ids.deviceLUID, VK_LUID_SIZE))
               << ",\"driver_info\":" << jsonQuote(driver.driverInfo) << ",\"driver_raw\":" << properties.driverVersion
               << ",\"driver_nvidia\":" << jsonQuote(std::to_string(properties.driverVersion >> 22) + "." +
                   std::to_string((properties.driverVersion >> 14) & 255) + "." + std::to_string((properties.driverVersion >> 6) & 255) + "." + std::to_string(properties.driverVersion & 63));
        auto getQueues = load<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(gipa, instance, "vkGetPhysicalDeviceQueueFamilyProperties");
        getQueues(physical, &count, nullptr); std::vector<VkQueueFamilyProperties> queues(count); getQueues(physical, &count, queues.data());
        queueIndex = 0;
        while (queueIndex < count && (queues[queueIndex].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ++queueIndex;
        if (queueIndex == count) throw std::runtime_error("No graphics/compute queue");
        auto enumDeviceExt = load<PFN_vkEnumerateDeviceExtensionProperties>(gipa, instance, "vkEnumerateDeviceExtensionProperties");
        checked(enumDeviceExt(physical, nullptr, &count, nullptr), "device extension count");
        std::vector<VkExtensionProperties> extensions(count); checked(enumDeviceExt(physical, nullptr, &count, extensions.data()), "device extensions");
        requiredCount = 0; required = nullptr;
        auto devRequirementResult = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physical, &discovery, &requiredCount, &required);
        fields << ",\"device_extension_query\":" << jsonQuote(resultHex(devRequirementResult));
        std::vector<std::string> names;
        if (NVSDK_NGX_SUCCEED(devRequirementResult))
            for (uint32_t i = 0; i < requiredCount; ++i) addExt(names, extensions, required[i].extensionName);
        for (const char* name : { "VK_NVX_binary_import", "VK_NVX_image_view_handle", "VK_KHR_push_descriptor",
                "VK_KHR_external_memory", "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore", "VK_KHR_external_semaphore_win32" }) addExt(names, extensions, name);
        if(NVSDK_NGX_SUCCEED(devRequirementResult)) for(uint32_t i=0;i<requiredCount;++i)
            if(std::find(names.begin(),names.end(),required[i].extensionName)==names.end()) throw std::runtime_error("Required device extension absent");
        std::vector<const char*> pointers; for (auto& name : names) pointers.push_back(name.c_str());
        fields << ",\"device_extensions\": [";
        for (size_t i = 0; i < names.size(); ++i) fields << (i ? "," : "") << jsonQuote(names[i]);
        fields << "]";
        fields << ",\"instance_extensions\": [";
        for(size_t i=0;i<instNames.size();++i)fields<<(i?",":"")<<jsonQuote(instNames[i]);fields<<"]";
        timestampBits=queues[queueIndex].timestampValidBits;timestampPeriod=properties.limits.timestampPeriod;
        r.raw("timestamp_valid_bits",std::to_string(timestampBits));r.raw("timestamp_period_ns",std::to_string(timestampPeriod));
        VkPhysicalDeviceVulkan12Features f12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        VkPhysicalDeviceFeatures2 supported{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 }; supported.pNext = &f12;
        load<PFN_vkGetPhysicalDeviceFeatures2>(gipa, instance, "vkGetPhysicalDeviceFeatures2")(physical, &supported);
        // Enable only a small subset of reported-supported features; no command execution.
        VkPhysicalDeviceVulkan12Features enabled12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        enabled12.bufferDeviceAddress = f12.bufferDeviceAddress; enabled12.timelineSemaphore = f12.timelineSemaphore;
        enabled12.shaderFloat16 = f12.shaderFloat16; enabled12.shaderInt8 = f12.shaderInt8;
        VkPhysicalDeviceFeatures enabled{};
        enabled.shaderInt16 = supported.features.shaderInt16;
        enabled.shaderStorageImageReadWithoutFormat = supported.features.shaderStorageImageReadWithoutFormat;
        enabled.shaderStorageImageWriteWithoutFormat = supported.features.shaderStorageImageWriteWithoutFormat;
        float priority = 1;
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = queueIndex; qci.queueCount = 1; qci.pQueuePriorities = &priority;
        VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        dci.pNext = &enabled12; dci.pEnabledFeatures = &enabled; dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = uint32_t(pointers.size()); dci.ppEnabledExtensionNames = pointers.data();
        checked(load<PFN_vkCreateDevice>(gipa, instance, "vkCreateDevice")(physical, &dci, nullptr, &device), "vkCreateDevice");
        gdpa = load<PFN_vkGetDeviceProcAddr>(gipa, instance, "vkGetDeviceProcAddr");
        destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(gdpa(device, "vkDestroyDevice"));

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
