#include <windows.h>
#include <psapi.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <nvsdk_ngx_helpers_dlssg_d3d.h>
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
#include <array>
#include <bit>
#include "evidence.inc"
#include "d3d12_session.inc"
#include "camera.inc"

struct MetricsResult {
    UINT totalPixels{};
    double rgbMae{};
    UINT bad32Count{};
    UINT bad64Count{};
    double bad32Fraction{};
    double bad64Fraction{};
    UINT challengingPixels{};
    double challengingMae{};
    UINT challengingBad32Count{};
    UINT challengingBad64Count{};
    double challengingBad32Fraction{};
    double challengingBad64Fraction{};
};

struct Compute {
    D3D12NgxSession& s;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cpuHeap;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> fixture, hybrid, reduction, qualityMetrics;
    Microsoft::WRL::ComPtr<ID3D12Resource> stats;
    UINT stride{};
    UINT cpuStride{};

    D3D12_CPU_DESCRIPTOR_HANDLE cpu(UINT i) {
        auto h = heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(i) * stride;
        return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE gpu(UINT i) {
        auto h = heap->GetGPUDescriptorHandleForHeapStart();
        h.ptr += UINT64(i) * stride;
        return h;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE cpuClear(UINT i) {
        auto h = cpuHeap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(i) * cpuStride;
        return h;
    }

    explicit Compute(D3D12NgxSession& session, fs::path shaders) : s(session) {
        // Root Signature: Table 0 = 7 SRVs (t0..t6), Table 1 = 4 UAVs (u0..u3), Constants = 5 (b0)
        D3D12_DESCRIPTOR_RANGE ranges[2]{};
        ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7, 0, 0, 0};
        ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, 0};
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
        Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
        s.check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), "Serialize root");
        s.check(s.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)), "Create root");

        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 12;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        s.check(s.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)), "Create descriptors");
        stride = s.device->GetDescriptorHandleIncrementSize(hd.Type);

        D3D12_DESCRIPTOR_HEAP_DESC chd{};
        chd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        chd.NumDescriptors = 12;
        chd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        s.check(s.device->CreateDescriptorHeap(&chd, IID_PPV_ARGS(&cpuHeap)), "Create non-shader-visible CPU descriptor heap");
        cpuStride = s.device->GetDescriptorHandleIncrementSize(chd.Type);

        auto make = [&](const char* name, auto& pso) {
            auto bytes = readFile(shaders / (std::string(name) + ".cso"));
            D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
            pd.pRootSignature = root.Get();
            pd.CS = {bytes.data(), bytes.size()};
            s.check(s.device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso)), name);
        };
        make("FixtureCS", fixture);
        make("HybridInterpolationCS", hybrid);
        make("ReductionCS", reduction);
        make("QualityMetricsCS", qualityMetrics);

        stats = s.buffer(256, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        for (UINT i = 0; i < 7; ++i) srv(i, nullptr, DXGI_FORMAT_R8G8B8A8_UNORM);
        uav(0, nullptr, DXGI_FORMAT_R8G8B8A8_UNORM);
        uav(1, nullptr, DXGI_FORMAT_R32_FLOAT);
        uav(2, nullptr, DXGI_FORMAT_R32G32_FLOAT);

        D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};
        ud.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        ud.Format = DXGI_FORMAT_R32_TYPELESS;
        ud.Buffer.NumElements = 64;
        ud.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        s.device->CreateUnorderedAccessView(stats.Get(), nullptr, &ud, cpu(10));
        s.device->CreateUnorderedAccessView(stats.Get(), nullptr, &ud, cpuClear(10));
        s.r.event("clear_uav_pair", "stats resource=" + handleText(stats.Get()) + " format=R32_TYPELESS dimension=BUFFER CLEAR_UAV_DESCRIPTOR_PAIR_VALID=YES");

        s.begin();
        s.transition(stats.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        UINT zero[4]{};
        s.commands->ClearUnorderedAccessViewUint(gpu(10), cpuClear(10), stats.Get(), zero, 0, nullptr);
        uavBarrier(stats.Get());
        s.complete("statistics initialize");
    }

    void srv(UINT i, ID3D12Resource* resource, DXGI_FORMAT format) {
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format = format;
        d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Texture2D.MipLevels = 1;
        s.device->CreateShaderResourceView(resource, &d, cpu(i));
    }
    void uav(UINT i, ID3D12Resource* resource, DXGI_FORMAT format) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
        d.Format = format;
        d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        s.device->CreateUnorderedAccessView(resource, nullptr, &d, cpu(7 + i));
        s.device->CreateUnorderedAccessView(resource, nullptr, &d, cpuClear(7 + i));
    }
    void bind() {
        ID3D12DescriptorHeap* heaps[] = {heap.Get()};
        s.commands->SetDescriptorHeaps(1, heaps);
        s.commands->SetComputeRootSignature(root.Get());
        s.commands->SetComputeRootDescriptorTable(0, gpu(0));
        s.commands->SetComputeRootDescriptorTable(1, gpu(7));
    }
    void constants(UINT frame, float t, UINT offset) {
        UINT v[] = {W, H, frame, std::bit_cast<UINT>(t), offset};
        s.commands->SetComputeRoot32BitConstants(2, 5, v, 0);
    }
    void uavBarrier(ID3D12Resource* resource) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.UAV.pResource = resource;
        s.commands->ResourceBarrier(1, &b);
    }

    void scene(D3DImage& color, D3DImage& depth, D3DImage& motion, float t, const std::string& label) {
        uav(0, color.resource.Get(), color.format);
        uav(1, depth.resource.Get(), depth.format);
        uav(2, motion.resource.Get(), motion.format);
        s.begin();
        s.barrier(color, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.barrier(depth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.barrier(motion, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        constants(0, t, 0);
        s.commands->SetPipelineState(fixture.Get());
        s.commands->Dispatch((W + 7) / 8, (H + 7) / 8, 1);
        s.barrier(color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(motion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.complete("GPU fixture " + label + " t=" + std::to_string(t));
    }

    void sentinel(D3DImage& image, const std::array<float, 4>& color, const std::string& label = "image") {
        uav(0, image.resource.Get(), image.format);
        s.r.event("clear_uav_pair", label + " resource=" + handleText(image.resource.Get()) + " format=" + std::to_string(image.format) + " dimension=TEXTURE2D CLEAR_UAV_DESCRIPTOR_PAIR_VALID=YES");
        s.begin();
        s.barrier(image, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        s.commands->ClearUnorderedAccessViewFloat(gpu(7), cpuClear(7), image.resource.Get(), color.data(), 0, nullptr);
        s.barrier(image, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.complete("GPU sentinel " + label);
    }

    void interpolate(D3DImage& a, D3DImage& b, D3DImage& da, D3DImage& db, D3DImage& ma, D3DImage& mb, D3DImage& output, float t) {
        D3DImage* inputs[] = {&a, &b, &ma, &mb, &da, &db};
        for (UINT i = 0; i < 6; ++i) srv(i, inputs[i]->resource.Get(), inputs[i]->format);
        uav(0, output.resource.Get(), output.format);
        s.begin();
        for (auto input : inputs) s.barrier(*input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        constants(0, t, 0);
        s.commands->SetPipelineState(hybrid.Get());
        s.commands->Dispatch((W + 7) / 8, (H + 7) / 8, 1);
        s.barrier(output, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.complete("Hybrid t=" + std::to_string(t));
    }

    void reduce(D3DImage& image, UINT slot) {
        srv(0, image.resource.Get(), image.format);
        s.begin();
        s.barrier(image, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        bind();
        constants(0, 0.0f, slot * 16);
        s.commands->SetPipelineState(reduction.Get());
        s.commands->Dispatch((W + 7) / 8, (H + 7) / 8, 1);
        uavBarrier(stats.Get());
        s.complete("GPU fingerprint slot " + std::to_string(slot));
    }

    void evaluateMetrics(D3DImage& cand, D3DImage& refCol, D3DImage& da, D3DImage& db, D3DImage& refZ, UINT byteOffset, const std::string& label) {
        srv(0, cand.resource.Get(), cand.format);
        srv(1, refCol.resource.Get(), refCol.format);
        srv(4, da.resource.Get(), da.format);
        srv(5, db.resource.Get(), db.format);
        srv(6, refZ.resource.Get(), refZ.format);
        s.begin();
        s.barrier(cand, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(refCol, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(da, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(db, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(refZ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        bind();
        constants(0, 0.0f, byteOffset);
        s.commands->SetPipelineState(qualityMetrics.Get());
        s.commands->Dispatch((W + 7) / 8, (H + 7) / 8, 1);
        uavBarrier(stats.Get());
        s.complete("Quality metrics " + label + " offset " + std::to_string(byteOffset));
    }

    std::array<UINT, 64> readStats() {
        auto readback = s.buffer(256, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        s.begin();
        s.transition(stats.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        s.commands->CopyBufferRegion(readback.Get(), 0, stats.Get(), 0, 256);
        s.transition(stats.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.complete("256-byte bounded statistics readback");
        void* p{};
        D3D12_RANGE range{0, 256};
        s.check(readback->Map(0, &range, &p), "Map statistics");
        std::array<UINT, 64> values{};
        memcpy(values.data(), p, 256);
        D3D12_RANGE empty{0, 0};
        readback->Unmap(0, &empty);
        return values;
    }
};

static MetricsResult parseMetrics(const std::array<UINT, 64>& values, UINT offsetIndex) {
    MetricsResult m;
    m.totalPixels = values[offsetIndex + 0];
    if (m.totalPixels > 0) {
        m.rgbMae = double(values[offsetIndex + 1]) / (4000.0 * m.totalPixels);
        m.bad32Count = values[offsetIndex + 2];
        m.bad64Count = values[offsetIndex + 3];
        m.bad32Fraction = double(m.bad32Count) / m.totalPixels;
        m.bad64Fraction = double(m.bad64Count) / m.totalPixels;
    }
    m.challengingPixels = values[offsetIndex + 4];
    if (m.challengingPixels > 0) {
        m.challengingMae = double(values[offsetIndex + 5]) / (4000.0 * m.challengingPixels);
        m.challengingBad32Count = values[offsetIndex + 6];
        m.challengingBad64Count = values[offsetIndex + 7];
        m.challengingBad32Fraction = double(m.challengingBad32Count) / m.challengingPixels;
        m.challengingBad64Fraction = double(m.challengingBad64Count) / m.challengingPixels;
    }
    return m;
}

int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 5) return 2;
    fs::path out = fs::absolute(argv[1]), dll = fs::absolute(argv[2]), runtime = fs::absolute(argv[3]), shaders = fs::absolute(argv[4]);
    fs::create_directories(out);
    vendorLog.open(out / "ngx-callback.log");
    validationLog.open(out / "d3d12-debug.log");
    validationMessagesLog.open(out / "validation-messages.log");
    EvidenceRecorder r(out);

    r.str("experiment", "HYBRID_X4_REAL_SCENE_QUALITY");
    r.str("github_start_head", "ff6b793afbd7f9b4096cc1bc2d54d2593336c9a0");
    r.raw("build_count_this_iteration", "1");
    r.raw("run_count_this_iteration", "1");
    r.str("ground_truth_used_as_generation_input", "NO");
    r.raw("clear_uav_descriptor_pair_valid", "true");
    r.raw("nvidia_generated_position", "0.5");
    r.raw("hybrid_generated_positions", "[0.25,0.75]");
    r.raw("nvidia_multiframe_count", "1");
    r.raw("nvidia_multiframe_index", "1");
    r.raw("cpu_transport_copy_count", "0");
    r.raw("bounded_readback_bytes", "272"); // 16 disable + 256 stats
    r.raw("present_executed", "false");
    r.raw("production_quality_interpolation", "false");

    // Pre-fixed Quality Thresholds (Fixed BEFORE execution)
    constexpr double OVERALL_RGB_MAE_LIMIT = 0.08;
    constexpr double OVERALL_BAD64_FRACTION_LIMIT = 0.10;
    constexpr double CHALLENGING_RGB_MAE_LIMIT = 0.18;
    constexpr double CHALLENGING_BAD64_FRACTION_LIMIT = 0.25;

    r.raw("overall_rgb_mae_limit", "0.08");
    r.raw("overall_bad64_fraction_limit", "0.10");
    r.raw("challenging_rgb_mae_limit", "0.18");
    r.raw("challenging_bad64_fraction_limit", "0.25");

    r.str("structural_validation", "NOT_RUN");
    r.str("custom_interpolator_quality", "NOT_RUN");
    r.str("nvidia_g50_quality", "NOT_RUN");
    r.str("quality_validation", "NOT_RUN");
    r.str("next_experiment", "STOP");

    ExternalLoader loader;
    D3D12NgxSession s(r, runtime);
    int exit = 1;
    std::string stage = "INITIALIZATION";

    try {
        loader.start(r, dll, 1);
        s.initialize();

        // Target Frames A & B, plus generated intermediates G25, G50, G75
        auto A = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "A"), B = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "B");
        auto G25 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G25"), G50 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G50"), G75 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G75");
        auto DA = s.image(DXGI_FORMAT_R32_FLOAT, 4, "DepthA"), DB = s.image(DXGI_FORMAT_R32_FLOAT, 4, "DepthB");
        auto MA = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionA"), MB = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionB");

        // Warmup buffers for DLSS FG
        auto C = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "warmupColor"), D = s.image(DXGI_FORMAT_R32_FLOAT, 4, "warmupDepth"), M = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "warmupMotion");

        // Ground Truth Reference frames (strictly for evaluation, NEVER generation input)
        auto REF25 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF25"), REF50 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF50"), REF75 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF75");
        auto D25 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D25"), D50 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D50"), D75 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D75");
        auto M25 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M25"), M50 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M50"), M75 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M75");

        Compute compute(s, shaders);

        // Step 1: Render Frame A (t=0.0) and Frame B (t=1.0)
        compute.scene(A, DA, MA, 0.0f, "A_t0");
        compute.scene(B, DB, MB, 1.0f, "B_t1");

        // Step 2: Initialize Sentinels
        compute.sentinel(G25, {1, 0, 1, 1}, "G25"); compute.reduce(G25, 5);
        compute.sentinel(G50, {1, 1, 1, 1}, "G50_init"); compute.reduce(G50, 6);
        compute.sentinel(G75, {0, 1, 1, 1}, "G75"); compute.reduce(G75, 7);

        // Step 3: NVIDIA DLSS FG Setup
        auto code = NVSDK_NGX_D3D12_AllocateParameters(&s.parameters);
        if (!NVSDK_NGX_SUCCEED(code) || !s.parameters) throw std::runtime_error("NGX AllocateParameters failed");
        NVSDK_NGX_DLSSG_Create_Params cp{};
        cp.Width = cp.RenderWidth = W; cp.Height = cp.RenderHeight = H;
        cp.NativeBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        NVSDK_NGX_Parameter_SetUI(s.parameters, NVSDK_NGX_DLSSG_Parameter_Width, W);
        NVSDK_NGX_Parameter_SetUI(s.parameters, NVSDK_NGX_DLSSG_Parameter_Height, H);

        s.begin();
        code = NGX_D3D12_CREATE_DLSSG(s.commands.Get(), 1, 1, &s.feature, s.parameters, &cp);
        r.str("nvidia_create_result", resultHex(code));
        if (!NVSDK_NGX_SUCCEED(code) || !s.feature) throw std::runtime_error("NVIDIA CreateFeature failed");
        s.complete("CreateFeature");

        auto disable = s.buffer(16, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        s.begin();
        s.transition(disable.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.complete("disable buffer initialize");

        // Step 4: DLSS FG Warmup and G50 Generation
        // Frames 0..2: warmup at t=-3.0, -2.0, -1.0
        // Frame 3: Frame A at t=0.0
        // Frame 4: Frame B at t=1.0 -> DLSS FG interpolates G50 between A and B
        for (UINT frame = 0; frame <= 4; ++frame) {
            D3DImage *color = &C, *depth = &D, *motion = &M;
            float frameT = float(int(frame) - 3);
            if (frame < 3) {
                compute.scene(C, D, M, frameT, "warmup_" + std::to_string(frame));
            } else if (frame == 3) {
                color = &A; depth = &DA; motion = &MA;
            } else {
                color = &B; depth = &DB; motion = &MB;
            }
            compute.sentinel(G50, {1, 1, 1, 1}, "G50_frame_" + std::to_string(frame));
            s.resetDisable(disable.Get());
            s.begin();
            s.barrier(*color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            s.barrier(*depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            s.barrier(*motion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            s.barrier(G50, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            NVSDK_NGX_D3D12_DLSSG_Eval_Params ep{};
            ep.pBackbuffer = color->resource.Get();
            ep.pHudless = color->resource.Get();
            ep.pDepth = depth->resource.Get();
            ep.pMVecs = motion->resource.Get();
            ep.pOutputInterpFrame = G50.resource.Get();
            ep.pOutputDisableInterpolation = disable.Get();

            auto options = FixtureCamera::options(frame == 0);
            options.cameraViewToClip[1][1] *= -1;
            options.clipToCameraView[1][1] *= -1;

            code = NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(), s.feature, s.parameters, &ep, &options);
            r.raw("nvidia_evaluate_count", std::to_string(frame + 1));
            r.str("nvidia_evaluate_result", resultHex(code));
            if (!NVSDK_NGX_SUCCEED(code)) throw std::runtime_error("NVIDIA Evaluate failed");
            s.complete(frame == 4 ? "x2 target" : "warmup");
            if (frame == 4) {
                r.raw("nvidia_target_evaluate_count", "1");
                r.raw("nvidia_warmup_evaluate_count", "4");
                r.str("nvidia_g50_gpu_completion", "PASS");
            }
        }
        unsigned status = s.readDisable(disable.Get());
        r.raw("nvidia_output_disable", std::to_string(status));
        if (status != 0) throw std::runtime_error("NVIDIA output disable flag is set");

        // Step 5: Generate G25 and G75 using baseline HybridInterpolationCS
        stage = "HYBRID_SHADER_GENERATION";
        compute.interpolate(A, B, DA, DB, MA, MB, G25, 0.25f);
        r.raw("hybrid_dispatch_count", "1");
        r.str("g25_gpu_completion", "PASS");

        compute.interpolate(A, B, DA, DB, MA, MB, G75, 0.75f);
        r.raw("hybrid_dispatch_count", "2");
        r.str("g75_gpu_completion", "PASS");

        // Step 6: Render Ground Truth References (Validation ONLY)
        compute.scene(REF25, D25, M25, 0.25f, "REF25");
        compute.scene(REF50, D50, M50, 0.50f, "REF50");
        compute.scene(REF75, D75, M75, 0.75f, "REF75");

        // Step 7: Run GPU Quality Metrics CS
        // G25 vs REF25 at offset 128 (index 32)
        compute.evaluateMetrics(G25, REF25, DA, DB, D25, 128, "G25_vs_REF25");
        // G50 vs REF50 at offset 160 (index 40)
        compute.evaluateMetrics(G50, REF50, DA, DB, D50, 160, "G50_vs_REF50");
        // G75 vs REF75 at offset 192 (index 48)
        compute.evaluateMetrics(G75, REF75, DA, DB, D75, 192, "G75_vs_REF75");

        // Step 8: Run Reduction CS for Fingerprints (slots 0..4, offsets 0, 16, 32, 48, 64)
        D3DImage* frames[] = {&A, &G25, &G50, &G75, &B};
        const char* names[] = {"a", "g25", "g50", "g75", "b"};
        for (UINT i = 0; i < 5; ++i) compute.reduce(*frames[i], i);

        // Step 9: 256-byte Bounded Statistics Readback
        auto values = compute.readStats();

        // Step 10: Process Fingerprints & Structural Verification
        std::array<std::string, 8> fingerprints{};
        std::array<double, 5> positions{};
        std::ofstream evidence(out / "frame-fingerprints.txt");
        for (UINT i = 0; i < 8; ++i) {
            std::ostringstream fp;
            fp << std::hex << std::setfill('0') << std::setw(8) << values[i * 4] << std::setw(8) << values[i * 4 + 1];
            fingerprints[i] = fp.str();
            if (i < 5) {
                r.str(std::string("hash_") + names[i], fp.str());
                auto mass = values[i * 4 + 3];
                positions[i] = mass ? double(values[i * 4 + 2]) / mass : std::numeric_limits<double>::quiet_NaN();
                r.raw(std::string("position_") + names[i], mass ? std::to_string(positions[i]) : "null");
                r.raw(std::string("object_pixels_") + names[i], std::to_string(mass));
                r.str(std::string("resource_") + names[i], handleText(frames[i]->resource.Get()));
                evidence << names[i] << " fingerprint=" << fp.str() << " position=" << positions[i] << " objectPixels=" << mass << " resource=" << frames[i]->resource.Get() << '\n';
            } else {
                evidence << "sentinel" << i - 5 << " fingerprint=" << fp.str() << '\n';
            }
        }
        r.str("fingerprint_method", "GPU FNV-per-pixel coordinate-keyed XOR + additive reduction, two uint32 words");

        bool changed = true, distinct = true, ordered = true, resourcesDistinct = true;
        for (UINT i = 1; i <= 3; ++i) {
            bool ch = fingerprints[i] != fingerprints[i + 4];
            bool ab = fingerprints[i] != fingerprints[0] && fingerprints[i] != fingerprints[4];
            changed &= ch;
            distinct &= ab;
            r.raw(std::string(names[i]) + "_changed_from_sentinel", ch ? "true" : "false");
            r.raw(std::string(names[i]) + "_distinct_from_a_b", ab ? "true" : "false");
            r.raw(std::string(names[i]) + "_generated", ch && values[i * 4 + 3] > 0 ? "true" : "false");
        }
        for (UINT i = 1; i <= 3; ++i) {
            for (UINT j = i + 1; j <= 3; ++j) {
                bool diff = fingerprints[i] != fingerprints[j];
                distinct &= diff;
                r.raw(std::string(names[i]) + "_distinct_from_" + names[j], diff ? "true" : "false");
            }
        }
        for (UINT i = 0; i < 5; ++i) {
            ordered &= std::isfinite(positions[i]) && values[i * 4 + 3] > 30000 && values[i * 4 + 3] < 70000;
            for (UINT j = i + 1; j < 5; ++j) resourcesDistinct &= (frames[i]->resource.Get() != frames[j]->resource.Get());
            if (i > 0) ordered &= (positions[i - 1] < positions[i]);
        }
        r.raw("resources_distinct", resourcesDistinct ? "true" : "false");

        double span = positions[4] - positions[0];
        double expG25 = positions[0] + 0.25 * span;
        double expG50 = positions[0] + 0.50 * span;
        double expG75 = positions[0] + 0.75 * span;
        double errG25 = std::abs(positions[1] - expG25);
        double errG50 = std::abs(positions[2] - expG50);
        double errG75 = std::abs(positions[3] - expG75);
        constexpr double tolerance = 2.0;
        bool withinTol = (errG25 <= tolerance) && (errG50 <= tolerance) && (errG75 <= tolerance);
        r.raw("g25_position_error_pixels", std::to_string(errG25));
        r.raw("g50_position_error_pixels", std::to_string(errG50));
        r.raw("g75_position_error_pixels", std::to_string(errG75));
        r.raw("temporal_position_tolerance_pixels", std::to_string(tolerance));
        r.raw("temporal_order_valid", (ordered && withinTol) ? "true" : "false");

        bool structuralPass = changed && distinct && ordered && withinTol && resourcesDistinct;
        r.str("structural_validation", structuralPass ? "PASS" : "FAIL");

        // Step 11: Parse Quality Metrics
        auto g25Metrics = parseMetrics(values, 32); // offset 128 / 4 = 32
        auto g50Metrics = parseMetrics(values, 40); // offset 160 / 4 = 40
        auto g75Metrics = parseMetrics(values, 48); // offset 192 / 4 = 48

        // Record metrics in EvidenceRecorder
        r.raw("g25_rgb_mae", std::to_string(g25Metrics.rgbMae));
        r.raw("g50_rgb_mae", std::to_string(g50Metrics.rgbMae));
        r.raw("g75_rgb_mae", std::to_string(g75Metrics.rgbMae));

        r.raw("g25_bad32_count", std::to_string(g25Metrics.bad32Count));
        r.raw("g50_bad32_count", std::to_string(g50Metrics.bad32Count));
        r.raw("g75_bad32_count", std::to_string(g75Metrics.bad32Count));

        r.raw("g25_bad32_fraction", std::to_string(g25Metrics.bad32Fraction));
        r.raw("g50_bad32_fraction", std::to_string(g50Metrics.bad32Fraction));
        r.raw("g75_bad32_fraction", std::to_string(g75Metrics.bad32Fraction));

        r.raw("g25_bad64_count", std::to_string(g25Metrics.bad64Count));
        r.raw("g50_bad64_count", std::to_string(g50Metrics.bad64Count));
        r.raw("g75_bad64_count", std::to_string(g75Metrics.bad64Count));

        r.raw("g25_bad64_fraction", std::to_string(g25Metrics.bad64Fraction));
        r.raw("g50_bad64_fraction", std::to_string(g50Metrics.bad64Fraction));
        r.raw("g75_bad64_fraction", std::to_string(g75Metrics.bad64Fraction));

        // Challenging region metrics
        r.raw("g25_challenging_pixels", std::to_string(g25Metrics.challengingPixels));
        r.raw("g50_challenging_pixels", std::to_string(g50Metrics.challengingPixels));
        r.raw("g75_challenging_pixels", std::to_string(g75Metrics.challengingPixels));

        r.raw("g25_challenging_rgb_mae", std::to_string(g25Metrics.challengingMae));
        r.raw("g50_challenging_rgb_mae", std::to_string(g50Metrics.challengingMae));
        r.raw("g75_challenging_rgb_mae", std::to_string(g75Metrics.challengingMae));

        r.raw("g25_challenging_bad32_fraction", std::to_string(g25Metrics.challengingBad32Fraction));
        r.raw("g50_challenging_bad32_fraction", std::to_string(g50Metrics.challengingBad32Fraction));
        r.raw("g75_challenging_bad32_fraction", std::to_string(g75Metrics.challengingBad32Fraction));

        r.raw("g25_challenging_bad64_fraction", std::to_string(g25Metrics.challengingBad64Fraction));
        r.raw("g50_challenging_bad64_fraction", std::to_string(g50Metrics.challengingBad64Fraction));
        r.raw("g75_challenging_bad64_fraction", std::to_string(g75Metrics.challengingBad64Fraction));

        // Write Quality Metrics JSON Report
        {
            std::ofstream qm(out / "quality-metrics.json");
            qm << std::fixed << std::setprecision(6);
            qm << "{\n";
            qm << "  \"thresholds\": {\n";
            qm << "    \"overall_rgb_mae_limit\": " << OVERALL_RGB_MAE_LIMIT << ",\n";
            qm << "    \"overall_bad64_fraction_limit\": " << OVERALL_BAD64_FRACTION_LIMIT << ",\n";
            qm << "    \"challenging_rgb_mae_limit\": " << CHALLENGING_RGB_MAE_LIMIT << ",\n";
            qm << "    \"challenging_bad64_fraction_limit\": " << CHALLENGING_BAD64_FRACTION_LIMIT << "\n";
            qm << "  },\n";
            qm << "  \"g25\": {\n";
            qm << "    \"rgb_mae\": " << g25Metrics.rgbMae << ",\n";
            qm << "    \"bad32_count\": " << g25Metrics.bad32Count << ",\n";
            qm << "    \"bad64_count\": " << g25Metrics.bad64Count << ",\n";
            qm << "    \"bad32_fraction\": " << g25Metrics.bad32Fraction << ",\n";
            qm << "    \"bad64_fraction\": " << g25Metrics.bad64Fraction << ",\n";
            qm << "    \"challenging_pixels\": " << g25Metrics.challengingPixels << ",\n";
            qm << "    \"challenging_rgb_mae\": " << g25Metrics.challengingMae << ",\n";
            qm << "    \"challenging_bad32_fraction\": " << g25Metrics.challengingBad32Fraction << ",\n";
            qm << "    \"challenging_bad64_fraction\": " << g25Metrics.challengingBad64Fraction << "\n";
            qm << "  },\n";
            qm << "  \"g50\": {\n";
            qm << "    \"rgb_mae\": " << g50Metrics.rgbMae << ",\n";
            qm << "    \"bad32_count\": " << g50Metrics.bad32Count << ",\n";
            qm << "    \"bad64_count\": " << g50Metrics.bad64Count << ",\n";
            qm << "    \"bad32_fraction\": " << g50Metrics.bad32Fraction << ",\n";
            qm << "    \"bad64_fraction\": " << g50Metrics.bad64Fraction << ",\n";
            qm << "    \"challenging_pixels\": " << g50Metrics.challengingPixels << ",\n";
            qm << "    \"challenging_rgb_mae\": " << g50Metrics.challengingMae << ",\n";
            qm << "    \"challenging_bad32_fraction\": " << g50Metrics.challengingBad32Fraction << ",\n";
            qm << "    \"challenging_bad64_fraction\": " << g50Metrics.challengingBad64Fraction << "\n";
            qm << "  },\n";
            qm << "  \"g75\": {\n";
            qm << "    \"rgb_mae\": " << g75Metrics.rgbMae << ",\n";
            qm << "    \"bad32_count\": " << g75Metrics.bad32Count << ",\n";
            qm << "    \"bad64_count\": " << g75Metrics.bad64Count << ",\n";
            qm << "    \"bad32_fraction\": " << g75Metrics.bad32Fraction << ",\n";
            qm << "    \"bad64_fraction\": " << g75Metrics.bad64Fraction << ",\n";
            qm << "    \"challenging_pixels\": " << g75Metrics.challengingPixels << ",\n";
            qm << "    \"challenging_rgb_mae\": " << g75Metrics.challengingMae << ",\n";
            qm << "    \"challenging_bad32_fraction\": " << g75Metrics.challengingBad32Fraction << ",\n";
            qm << "    \"challenging_bad64_fraction\": " << g75Metrics.challengingBad64Fraction << "\n";
            qm << "  }\n";
            qm << "}\n";
        }

        // Evaluate Quality Gates
        bool g25OverallPass = (g25Metrics.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (g25Metrics.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        bool g25ChallengingPass = (g25Metrics.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (g25Metrics.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);

        bool g75OverallPass = (g75Metrics.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (g75Metrics.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        bool g75ChallengingPass = (g75Metrics.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (g75Metrics.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);

        bool g50OverallPass = (g50Metrics.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (g50Metrics.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        bool g50ChallengingPass = (g50Metrics.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (g50Metrics.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);

        std::string customQuality;
        if (g25OverallPass && g25ChallengingPass && g75OverallPass && g75ChallengingPass) {
            customQuality = "PASS";
        } else if (g25OverallPass && g75OverallPass) {
            customQuality = "FAIL_OCCLUSION_REGIONS";
        } else {
            customQuality = "FAIL_GLOBAL_REPROJECTION";
        }
        r.str("custom_interpolator_quality", customQuality);

        std::string nvidiaQuality = (g50OverallPass && g50ChallengingPass) ? "PASS" : (g50OverallPass ? "FAIL_CHALLENGING_REGIONS" : "FAIL_GLOBAL");
        r.str("nvidia_g50_quality", nvidiaQuality);

        bool allPass = (customQuality == "PASS") && (nvidiaQuality == "PASS");
        r.str("quality_validation", allPass ? "PASS" : "FAIL");

        // Determine Next Experiment
        std::string nextExp;
        if (allPass) {
            nextExp = "HYBRID_X4_VISUAL_CAPTURE";
        } else if (customQuality == "FAIL_OCCLUSION_REGIONS") {
            nextExp = "HYBRID_X4_OCCLUSION_RECONSTRUCTION";
        } else {
            nextExp = "HYBRID_X4_MOTION_REPROJECTION_IMPROVEMENT";
        }
        r.str("next_experiment", nextExp);

        // Verification checks
        s.debugMessages();
        r.raw("d3d12_validation_errors", std::to_string(validationErrors));
        if (validationErrors > 0) {
            stage = "D3D12_VALIDATION";
            throw std::runtime_error("D3D12 validation errors occurred");
        }
        if (!structuralPass) {
            stage = "STRUCTURAL_VALIDATION_FAILED";
            throw std::runtime_error("Structural validation gate failed");
        }

        stage = "NONE";
        exit = 0;
    } catch (const std::exception& e) {
        s.debugMessages();
        r.raw("d3d12_validation_errors", std::to_string(validationErrors));
        r.str("error", e.what());
        r.event("failure", e.what());
    }

    if (s.device) {
        auto removed = s.device->GetDeviceRemovedReason();
        s.removed = FAILED(removed);
        r.raw("device_lost", s.removed ? "true" : "false");
        r.raw("device_removed", s.removed ? "true" : "false");
        r.str("device_removed_reason", hrHex(removed));
        if (s.removed) { exit = 1; stage = "DEVICE_LOST"; }
    }
    s.close();

    r.str("fail_stage", stage);
    r.raw("process_exit_code", std::to_string(exit));
    r.flush();

    std::cout << "EXIT=" << exit << " STAGE=" << stage << '\n';
    return exit;
}
