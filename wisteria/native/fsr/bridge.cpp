#include <windows.h>
#include <jni.h>
#include <ffx_api/ffx_api_loader.h>
#include <ffx_api/ffx_framegeneration.h>
#include <ffx_api/vk/ffx_api_vk.h>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <unordered_set>

namespace {
    HMODULE sdkModule = nullptr;
    ffxFunctions api{};
    std::string version;
    std::mutex contextsMutex;
    struct Session {
        ffxContext context = nullptr;
        ffxCreateBackendVKDesc backend{};
        ffxCreateContextDescFrameGeneration create{};
        ffxCreateContextDescFrameGenerationHudless hudless{};
        DWORD owner = GetCurrentThreadId();
    };
    std::unordered_set<Session*> contexts;
    void fail(JNIEnv* env, const std::string& text) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), text.c_str());
    }
    Session* session(JNIEnv* env, jlong pointer) {
        std::lock_guard lock(contextsMutex);
        auto* result = reinterpret_cast<Session*>(pointer);
        if (!contexts.contains(result) || result->owner != GetCurrentThreadId()) {
            fail(env, "Invalid FSR session or non-owning worker thread"); return nullptr;
        }
        return result;
    }
    FfxApiResource resource(JNIEnv* env, jlongArray tuple, bool output = false) {
        if (!tuple || env->GetArrayLength(tuple) != 4) { fail(env, "Invalid resource tuple"); return {}; }
        jlong t[4]{}; env->GetLongArrayRegion(tuple, 0, 4, t);
        if (!t[0] || t[2] <= 0 || t[3] <= 0) { fail(env, "Invalid Vulkan image/extent"); return {}; }
        FfxApiResourceDescription desc{};
        desc.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
        desc.format = ffxApiGetSurfaceFormatVK(static_cast<VkFormat>(t[1]));
        desc.width = static_cast<uint32_t>(t[2]); desc.height = static_cast<uint32_t>(t[3]);
        desc.depth = 1; desc.mipCount = 1;
        desc.usage = output ? FFX_API_RESOURCE_USAGE_UAV : FFX_API_RESOURCE_USAGE_READ_ONLY;
        return ffxApiGetResourceVK(reinterpret_cast<void*>(t[0]), desc,
            output ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS : FFX_API_RESOURCE_STATE_COMPUTE_READ);
    }
}

extern "C" {
JNIEXPORT void JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_initializeSdk(JNIEnv* env, jclass, jstring path) {
    if (sdkModule) return;
    const auto* chars = env->GetStringChars(path, nullptr);
    std::wstring filename(reinterpret_cast<const wchar_t*>(chars), env->GetStringLength(path));
    env->ReleaseStringChars(path, chars);
    sdkModule = LoadLibraryExW(filename.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!sdkModule) { fail(env, "Cannot load pinned amd_fidelityfx_vk.dll: Win32=" + std::to_string(GetLastError())); return; }
    ffxLoadFunctions(&api, sdkModule);
    if (!api.CreateContext || !api.DestroyContext || !api.Configure || !api.Query || !api.Dispatch) {
        fail(env, "Official FFX API exports missing"); return;
    }
    uint64_t count = 0;
    ffxQueryDescGetVersions query{};
    query.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
    query.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
    query.outputCount = &count;
    auto result = api.Query(nullptr, &query.header);
    if (result != FFX_API_RETURN_OK || !count) { fail(env, "No Vulkan frame-generation provider in official DLL, code=" + std::to_string(result)); return; }
    uint64_t ids[8]{}; const char* names[8]{};
    count = 8; query.versionIds = ids; query.versionNames = names;
    result = api.Query(nullptr, &query.header);
    if (result || !count || !names[0]) { fail(env, "FFX provider version query failed"); return; }
    // Package identity is independently SHA-256 pinned in Java; provider versions are effect ABI versions.
    version = "SDK 1.1.4; FG provider=" + std::string(names[0]) + "; id=" + std::to_string(ids[0]);
}
JNIEXPORT jint JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_queryBridgeAbi(JNIEnv*, jclass) { return 1; }
JNIEXPORT jstring JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_querySdkVersion(JNIEnv* env, jclass) { return env->NewStringUTF(version.c_str()); }
JNIEXPORT jint JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_queryCapabilities(JNIEnv*, jclass) { return !version.empty() && api.CreateContext && api.Dispatch ? 1 : 0; }
JNIEXPORT jlong JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_createSession(JNIEnv* env, jclass,
    jlong device, jlong physical, jlong getProc, jint width, jint height, jint rw, jint rh, jint format, jint hudlessFormat) {
    if (!api.CreateContext || !device || !physical || !getProc || width <= 0 || height <= 0 || rw <= 0 || rh <= 0) { fail(env, "Invalid FSR device/dimensions"); return 0; }
    auto value = std::make_unique<Session>();
    value->backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
    value->backend.vkDevice = reinterpret_cast<VkDevice>(device);
    value->backend.vkPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(physical);
    value->backend.vkDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(getProc);
    value->hudless.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_HUDLESS;
    value->hudless.header.pNext = &value->backend.header;
    value->hudless.hudlessBackBufferFormat = ffxApiGetSurfaceFormatVK(static_cast<VkFormat>(hudlessFormat));
    value->create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
    value->create.header.pNext = &value->hudless.header;
    // One FG-worker queue records prepare+dispatch; SR controls the submission and presentation.
    value->create.flags = FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING;
    value->create.displaySize = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    value->create.maxRenderSize = {static_cast<uint32_t>(rw), static_cast<uint32_t>(rh)};
    value->create.backBufferFormat = ffxApiGetSurfaceFormatVK(static_cast<VkFormat>(format));
    const auto result = api.CreateContext(&value->context, &value->create.header, nullptr);
    if (result != FFX_API_RETURN_OK) {
        if (value->context) api.DestroyContext(&value->context, nullptr);
        fail(env, "ffxCreateContext(Vulkan FG 3.1.4) code=" + std::to_string(result)); return 0;
    }
    std::lock_guard lock(contextsMutex);
    contexts.insert(value.get());
    return reinterpret_cast<jlong>(value.release());
}
JNIEXPORT void JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_destroySession(JNIEnv* env, jclass, jlong pointer) {
    auto* value = session(env, pointer);
    if (!value) return;
    const auto result = api.DestroyContext(&value->context, nullptr);
    if (result) { fail(env, "ffxDestroyContext code=" + std::to_string(result)); return; }
    { std::lock_guard lock(contextsMutex); contexts.erase(value); }
    delete value;
}
JNIEXPORT void JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_prepare(JNIEnv* env, jclass,
    jlong pointer, jlong commandBuffer, jlong frameId, jfloatArray params, jlongArray depth, jlongArray motion, jlongArray hudless) {
    auto* value = session(env, pointer); if (!value) return;
    if (!commandBuffer || !params || env->GetArrayLength(params) != 21) { fail(env, "Invalid prepare parameters"); return; }
    float p[21]{}; env->GetFloatArrayRegion(params, 0, 21, p);
    auto d = resource(env, depth), mv = resource(env, motion), h = resource(env, hudless);
    if (env->ExceptionCheck()) return;
    ffxConfigureDescFrameGeneration config{};
    config.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
    config.frameGenerationEnabled = true;
    config.allowAsyncWorkloads = false; // Application worker owns a single ordered compute stream.
    config.HUDLessColor = h;
    config.flags = FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY;
    config.generationRect = {0, 0, static_cast<int32_t>(value->create.displaySize.width), static_cast<int32_t>(value->create.displaySize.height)};
    config.frameID = static_cast<uint64_t>(frameId);
    auto code = api.Configure(&value->context, &config.header);
    if (code) { fail(env, "ffxConfigure FG code=" + std::to_string(code)); return; }
    ffxDispatchDescFrameGenerationPrepareCameraInfo camera{};
    camera.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_CAMERAINFO;
    for (int i = 0; i < 3; ++i) {
        camera.cameraPosition[i] = p[9 + i]; camera.cameraUp[i] = p[12 + i];
        camera.cameraRight[i] = p[15 + i]; camera.cameraForward[i] = p[18 + i];
    }
    ffxDispatchDescFrameGenerationPrepare prepare{};
    prepare.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE;
    prepare.header.pNext = &camera.header;
    prepare.frameID = static_cast<uint64_t>(frameId);
    prepare.commandList = reinterpret_cast<void*>(commandBuffer);
    prepare.renderSize = {d.description.width, d.description.height};
    prepare.jitterOffset = {p[0], p[1]}; prepare.motionVectorScale = {p[2], p[3]};
    prepare.frameTimeDelta = p[4]; prepare.cameraNear = p[5]; prepare.cameraFar = p[6];
    prepare.cameraFovAngleVertical = p[7]; prepare.viewSpaceToMetersFactor = p[8];
    prepare.depth = d; prepare.motionVectors = mv;
    code = api.Dispatch(&value->context, &prepare.header);
    if (code) fail(env, "ffxPrepare FG code=" + std::to_string(code));
}
JNIEXPORT void JNICALL Java_org_ireallywanttosleep_wisteria_fsr_FidelityFxBridge_generate(JNIEnv* env, jclass,
    jlong pointer, jlong commandBuffer, jlong frameId, jlongArray color, jlongArray output, jboolean reset) {
    auto* value = session(env, pointer); if (!value) return;
    ffxDispatchDescFrameGeneration dispatch{};
    dispatch.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION;
    dispatch.commandList = reinterpret_cast<void*>(commandBuffer);
    dispatch.presentColor = resource(env, color);
    dispatch.outputs[0] = resource(env, output, true);
    if (env->ExceptionCheck()) return;
    dispatch.numGeneratedFrames = 1;
    dispatch.reset = reset != JNI_FALSE;
    // Java validates Iris's explicit SRGB declaration. HDR luminance is unavailable and unused.
    dispatch.backbufferTransferFunction = FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
    dispatch.generationRect = {0, 0, static_cast<int32_t>(value->create.displaySize.width), static_cast<int32_t>(value->create.displaySize.height)};
    dispatch.frameID = static_cast<uint64_t>(frameId);
    const auto code = api.Dispatch(&value->context, &dispatch.header);
    if (code) fail(env, "ffxDispatch FG code=" + std::to_string(code));
}
}
