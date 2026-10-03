#include <windows.h>
#include <psapi.h>
#include <vulkan/vulkan.h>
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_defs_dlssg.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <mutex>
#include <stdexcept>
#include <algorithm>

namespace fs = std::filesystem;
static std::mutex logMutex;
static std::ofstream vendorLog;
static void NVSDK_CONV ngxLog(const char* message, NVSDK_NGX_Logging_Level, NVSDK_NGX_Feature) {
    std::lock_guard lock(logMutex);
    if (vendorLog) { vendorLog << message << '\n'; vendorLog.flush(); }
}
static std::string quoted(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char ch : value) {
        if (ch == '"' || ch == '\\') out << '\\' << ch;
        else if (ch == '\n') out << "\\n";
        else if (ch == '\r') out << "\\r";
        else if (ch < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(ch);
        else out << ch;
    }
    return out.str() + '"';
}
static std::string hexBytes(const uint8_t* bytes, size_t size) {
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) out << std::setw(2) << unsigned(bytes[i]);
    return out.str();
}
static std::string resultHex(NVSDK_NGX_Result result) {
    std::ostringstream out; out << "0x" << std::hex << std::setw(8) << std::setfill('0') << uint32_t(result);
    return out.str();
}
static void checked(VkResult result, const char* name) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(name) + " VkResult=" + std::to_string(result));
}
template<class F> F load(PFN_vkGetInstanceProcAddr gipa, VkInstance instance, const char* name) {
    auto fn = reinterpret_cast<F>(gipa(instance, name));
    if (!fn) throw std::runtime_error(std::string("Missing Vulkan function ") + name);
    return fn;
}
struct Getter {
    NVSDK_NGX_Result result = NVSDK_NGX_Result_FAIL_NotInitialized;
    unsigned int value = 0;
    std::string json() const {
        return "{\"result\":" + quoted(resultHex(result)) + ",\"value\":" +
            (NVSDK_NGX_SUCCEED(result) ? std::to_string(value) : "null") + "}";
    }
};

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) { std::cerr << "Usage: ngx_stock_probe OUTPUT_DIRECTORY STOCK_RUNTIME_DIRECTORY\n"; return 2; }
    fs::path output = fs::absolute(argv[1]), runtime = fs::absolute(argv[2]);
    fs::create_directories(output);
    vendorLog.open(output / "ngx-callback.log", std::ios::trunc);
    HMODULE vulkan = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyInstance destroyInstance = nullptr;
    PFN_vkDestroyDevice destroyDevice = nullptr;
    bool initialized = false;
    std::ostringstream fields;
    fields << "\"scenario\":\"stock\",\"evaluate_called\":false,\"generated_frames\":0";
    int status = 1;
    try {
        vulkan = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!vulkan) throw std::runtime_error("System Vulkan loader unavailable");
        auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkan, "vkGetInstanceProcAddr"));
        if (!gipa) throw std::runtime_error("vkGetInstanceProcAddr unavailable");
        const std::wstring runtimePath = runtime.wstring(), logPath = output.wstring();
        const wchar_t* runtimePaths[] = { runtimePath.c_str() };
        NVSDK_NGX_FeatureCommonInfo common{};
        common.PathListInfo = { runtimePaths, 1 };
        common.LoggingInfo = { ngxLog, NVSDK_NGX_LOGGING_LEVEL_VERBOSE, true };
        // Identifier belongs to this standalone CUSTOM probe; no NVIDIA sample/game AppID.
        const char* projectId = "72e7a53d-cb8a-4d01-bd3c-98f139a246e5";
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
        fields << ",\"instance_extension_query\":" << quoted(resultHex(instRequirementResult));
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
        app.pApplicationName = "Wisteria isolated NGX stock capability probe"; app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        ici.pApplicationInfo = &app; ici.enabledExtensionCount = uint32_t(instPointers.size()); ici.ppEnabledExtensionNames = instPointers.data();
        checked(load<PFN_vkCreateInstance>(gipa, {}, "vkCreateInstance")(&ici, nullptr, &instance), "vkCreateInstance");
        destroyInstance = load<PFN_vkDestroyInstance>(gipa, instance, "vkDestroyInstance");
        auto enumGpu = load<PFN_vkEnumeratePhysicalDevices>(gipa, instance, "vkEnumeratePhysicalDevices");
        checked(enumGpu(instance, &count, nullptr), "physical device count");
        std::vector<VkPhysicalDevice> gpus(count); checked(enumGpu(instance, &count, gpus.data()), "physical devices");
        VkPhysicalDevice physical = {};
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
        fields << ",\"gpu\":" << quoted(properties.deviceName) << ",\"device_uuid\":" << quoted(hexBytes(ids.deviceUUID, VK_UUID_SIZE))
               << ",\"device_luid_valid\":" << (ids.deviceLUIDValid ? "true" : "false")
               << ",\"device_luid\":" << quoted(hexBytes(ids.deviceLUID, VK_LUID_SIZE))
               << ",\"driver_info\":" << quoted(driver.driverInfo) << ",\"driver_raw\":" << properties.driverVersion
               << ",\"driver_nvidia\":" << quoted(std::to_string(properties.driverVersion >> 22) + "." +
                   std::to_string((properties.driverVersion >> 14) & 255) + "." + std::to_string((properties.driverVersion >> 6) & 255) + "." + std::to_string(properties.driverVersion & 63));
        auto getQueues = load<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(gipa, instance, "vkGetPhysicalDeviceQueueFamilyProperties");
        getQueues(physical, &count, nullptr); std::vector<VkQueueFamilyProperties> queues(count); getQueues(physical, &count, queues.data());
        uint32_t queueIndex = 0;
        while (queueIndex < count && (queues[queueIndex].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ++queueIndex;
        if (queueIndex == count) throw std::runtime_error("No graphics/compute queue");
        auto enumDeviceExt = load<PFN_vkEnumerateDeviceExtensionProperties>(gipa, instance, "vkEnumerateDeviceExtensionProperties");
        checked(enumDeviceExt(physical, nullptr, &count, nullptr), "device extension count");
        std::vector<VkExtensionProperties> extensions(count); checked(enumDeviceExt(physical, nullptr, &count, extensions.data()), "device extensions");
        requiredCount = 0; required = nullptr;
        auto devRequirementResult = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physical, &discovery, &requiredCount, &required);
        fields << ",\"device_extension_query\":" << quoted(resultHex(devRequirementResult));
        std::vector<std::string> names;
        if (NVSDK_NGX_SUCCEED(devRequirementResult))
            for (uint32_t i = 0; i < requiredCount; ++i) addExt(names, extensions, required[i].extensionName);
        for (const char* name : { "VK_NVX_binary_import", "VK_NVX_image_view_handle", "VK_KHR_push_descriptor",
                "VK_KHR_external_memory", "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore", "VK_KHR_external_semaphore_win32" }) addExt(names, extensions, name);
        std::vector<const char*> pointers; for (auto& name : names) pointers.push_back(name.c_str());
        fields << ",\"device_extensions\": [";
        for (size_t i = 0; i < names.size(); ++i) fields << (i ? "," : "") << quoted(names[i]);
        fields << "]";
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
        auto gdpa = load<PFN_vkGetDeviceProcAddr>(gipa, instance, "vkGetDeviceProcAddr");
        destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(gdpa(device, "vkDestroyDevice"));
        auto init = NVSDK_NGX_VULKAN_Init_with_ProjectID(projectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0.0", logPath.c_str(), instance, physical, device, gipa, gdpa, &common);
        initialized = NVSDK_NGX_SUCCEED(init);
        fields << ",\"ngx_init\":" << quoted(resultHex(init));
        if (!initialized) throw std::runtime_error("NGX init failed; getters not called");
        NVSDK_NGX_Parameter* parameters = nullptr;
        auto capabilities = NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters);
        fields << ",\"capability_query\":" << quoted(resultHex(capabilities));
        if (!NVSDK_NGX_SUCCEED(capabilities) || !parameters) throw std::runtime_error("Capability map unavailable");
        Getter available, featureInit, needsDriver, maximum;
        available.result = NVSDK_NGX_Parameter_GetUI(parameters, NVSDK_NGX_Parameter_FrameGeneration_Available, &available.value);
        int featureResult = 0;
        featureInit.result = NVSDK_NGX_Parameter_GetI(parameters, NVSDK_NGX_Parameter_FrameGeneration_FeatureInitResult, &featureResult);
        featureInit.value = static_cast<unsigned int>(featureResult);
        needsDriver.result = NVSDK_NGX_Parameter_GetUI(parameters, NVSDK_NGX_Parameter_FrameGeneration_NeedsUpdatedDriver, &needsDriver.value);
        maximum.result = NVSDK_NGX_Parameter_GetUI(parameters, NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax, &maximum.value);
        fields << ",\"available\":" << available.json() << ",\"feature_init_result\":" << featureInit.json()
               << ",\"needs_updated_driver\":" << needsDriver.json() << ",\"multi_frame_count_max\":" << maximum.json();
        fields << ",\"derived_multiplier_arithmetic_only\":" << (NVSDK_NGX_SUCCEED(maximum.result) && maximum.value > 0 ? std::to_string(uint64_t(maximum.value) + 1) : "null");
        fields << ",\"create_feature\":" << quoted(NVSDK_NGX_SUCCEED(available.result) && available.value == 1 ? "PENDING_AVAILABLE_GATE_PASSED" : "SKIPPED_FG_UNAVAILABLE");
        auto destroyParams = NVSDK_NGX_VULKAN_DestroyParameters(parameters);
        fields << ",\"destroy_parameters\":" << quoted(resultHex(destroyParams));
        // Observe actual module paths: the provided candidate is not necessarily the one chosen by NGX.
        HMODULE modules[1024]; DWORD bytes = 0;
        fields << ",\"ngx_modules\":[";
        bool first = true;
        if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytes)) {
            for (DWORD i = 0; i < std::min<DWORD>(bytes / sizeof(HMODULE), 1024); ++i) {
                wchar_t path[MAX_PATH];
                if (GetModuleFileNameExW(GetCurrentProcess(), modules[i], path, MAX_PATH)) {
                    std::string name = fs::path(path).filename().string();
                    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(tolower(c)); });
                    if (name.find("ngx") != std::string::npos || name.find("nvapi") != std::string::npos)
                        { fields << (first ? "" : ",") << quoted(fs::path(path).string()); first = false; }
                }
            }
        }
        fields << "]";
        status = NVSDK_NGX_SUCCEED(available.result) ? 0 : 1;
        fields << ",\"probe_status\":" << quoted(status == 0 ? "QUERY_COMPLETED" : "GETTER_FAILED");
    } catch (const std::exception& error) {
        fields << ",\"probe_status\":\"BLOCKED\",\"error\":" << quoted(error.what());
    }
    if (initialized) fields << ",\"ngx_shutdown\":" << quoted(resultHex(NVSDK_NGX_VULKAN_Shutdown1(device)));
    if (device && destroyDevice) destroyDevice(device, nullptr);
    if (instance && destroyInstance) destroyInstance(instance, nullptr);
    if (vulkan) FreeLibrary(vulkan);
    std::ofstream file(output / "raw-result.json"); file << "{" << fields.str() << "}\n";
    std::cout << "{" << fields.str() << "}\n";
    return status;
}
