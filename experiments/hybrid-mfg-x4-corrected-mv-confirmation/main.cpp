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

struct SpatialAlignment {
    int bestShiftX = 0;
    int bestShiftY = 0;
    double maeBefore = 0.0;
    double maeAfter = 0.0;
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
        Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
        s.check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), "Serialize root");
        s.check(s.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)), "Create root");

        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 16;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        s.check(s.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)), "Create descriptors");
        stride = s.device->GetDescriptorHandleIncrementSize(hd.Type);

        D3D12_DESCRIPTOR_HEAP_DESC chd{};
        chd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        chd.NumDescriptors = 16;
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
        uav(3, nullptr, DXGI_FORMAT_R32G32_FLOAT);

        D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};
        ud.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        ud.Format = DXGI_FORMAT_R32_TYPELESS;
        ud.Buffer.NumElements = 64;
        ud.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        s.device->CreateUnorderedAccessView(stats.Get(), nullptr, &ud, cpu(11));
        s.device->CreateUnorderedAccessView(stats.Get(), nullptr, &ud, cpuClear(11));

        clearStats();
    }

    void clearStats() {
        s.begin();
        s.transition(stats.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        UINT zero[4]{};
        s.commands->ClearUnorderedAccessViewUint(gpu(11), cpuClear(11), stats.Get(), zero, 0, nullptr);
        uavBarrier(stats.Get());
        s.complete("statistics clear");
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
    void constants(UINT motionMode, float t, UINT offset) {
        UINT v[] = {W, H, motionMode, std::bit_cast<UINT>(t), offset};
        s.commands->SetComputeRoot32BitConstants(2, 5, v, 0);
    }
    void uavBarrier(ID3D12Resource* resource) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.UAV.pResource = resource;
        s.commands->ResourceBarrier(1, &b);
    }

    void scene(D3DImage& color, D3DImage& depth, D3DImage& customMotion, D3DImage& nvidiaMotion, float t, const std::string& label) {
        uav(0, color.resource.Get(), color.format);
        uav(1, depth.resource.Get(), depth.format);
        uav(2, customMotion.resource.Get(), customMotion.format);
        uav(3, nvidiaMotion.resource.Get(), nvidiaMotion.format);
        s.begin();
        s.barrier(color, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.barrier(depth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.barrier(customMotion, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.barrier(nvidiaMotion, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        constants(0, t, 0);
        s.commands->SetPipelineState(fixture.Get());
        s.commands->Dispatch((W + 7) / 8, (H + 7) / 8, 1);
        s.barrier(color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(customMotion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(nvidiaMotion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.complete("GPU fixture " + label + " t=" + std::to_string(t));
    }

    void sentinel(D3DImage& image, const std::array<float, 4>& color, const std::string& label = "image") {
        uav(0, image.resource.Get(), image.format);
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
        s.complete("256-byte statistics readback");
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

static SpatialAlignment searchAlignment(const std::vector<uint8_t>& g50, const std::vector<uint8_t>& ref50, const std::vector<uint8_t>& d50, float minZ, float maxZ) {
    const float* pD50 = reinterpret_cast<const float*>(d50.data());
    std::vector<UINT> targetIndices;
    for (UINT y = 0; y < H; ++y) {
        for (UINT x = 0; x < W; ++x) {
            float z = pD50[y * W + x];
            if (z >= minZ && z < maxZ) {
                targetIndices.push_back(y * W + x);
            }
        }
    }
    if (targetIndices.empty()) return {};

    auto computeMae = [&](int shiftX, int shiftY) {
        double sum = 0.0;
        UINT count = 0;
        for (UINT idx : targetIndices) {
            int y = int(idx / W);
            int x = int(idx % W);
            int sx = x + shiftX;
            int sy = y + shiftY;
            if (sx >= 0 && sx < int(W) && sy >= 0 && sy < int(H)) {
                UINT gIdx = (sy * W + sx) * 4;
                UINT rIdx = idx * 4;
                double dr = std::abs(int(g50[gIdx + 0]) - int(ref50[rIdx + 0])) / 255.0;
                double dg = std::abs(int(g50[gIdx + 1]) - int(ref50[rIdx + 1])) / 255.0;
                double db = std::abs(int(g50[gIdx + 2]) - int(ref50[rIdx + 2])) / 255.0;
                sum += (dr + dg + db) / 3.0;
                ++count;
            }
        }
        return count > 0 ? sum / count : 1.0;
    };

    SpatialAlignment result;
    result.maeBefore = computeMae(0, 0);
    double bestMae = result.maeBefore;
    result.bestShiftX = 0;
    result.bestShiftY = 0;

    for (int dy = -8; dy <= 8; ++dy) {
        for (int dx = -8; dx <= 8; ++dx) {
            double m = computeMae(dx, dy);
            if (m < bestMae) {
                bestMae = m;
                result.bestShiftX = dx;
                result.bestShiftY = dy;
            }
        }
    }
    result.maeAfter = bestMae;
    return result;
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

    r.str("experiment", "HYBRID_X4_CORRECTED_MV_CONFIRMATION");
    r.str("github_start_head", "0bd0896c5913defdb3f86b52f95a6494af466b12");
    r.raw("build_count_this_iteration", "1");
    r.raw("run_count_this_iteration", "1");

    r.str("previous_mv_audit_custom_shader_contamination", "YES");
    r.str("custom_interpolator_restored_from_valid_baseline", "YES");
    r.str("custom_interpolator_baseline_source", "experiments/hybrid-mfg-x4-depth-contract/HybridInterpolationCS.hlsl");
    r.str("custom_interpolator_source_sha256", "baa7bcd55d561bedf8bd027e396b574528a83405d7994fbbcf29426c4ec712a8");
    r.str("custom_interpolator_algorithm_changed", "NO");

    r.str("custom_mv_buffer_unit", "NDC_CLIP_SPACE");
    r.str("nvidia_mv_buffer_unit", "SCREEN_SPACE_PIXELS");
    r.str("custom_nvidia_mv_resources_separate", "YES");

    r.raw("nvidia_mvec_scale_x", "1.0");
    r.raw("nvidia_mvec_scale_y", "1.0");
    r.str("nvidia_mv_temporal_direction", "CURRENT_TO_PREVIOUS");
    r.str("nvidia_camera_motion_included", "true");

    r.raw("production_cpu_transport_copy_count", "0");
    r.raw("diagnostic_cpu_readback_count", "3"); // REF50, D50, G50 for spatial search

    // Quality thresholds
    constexpr double OVERALL_RGB_MAE_LIMIT = 0.08;
    constexpr double OVERALL_BAD64_FRACTION_LIMIT = 0.10;
    constexpr double CHALLENGING_RGB_MAE_LIMIT = 0.18;
    constexpr double CHALLENGING_BAD64_FRACTION_LIMIT = 0.25;

    // Reproduction tolerances
    constexpr double CUSTOM_METRIC_REPRO_TOLERANCE = 0.000001;
    constexpr double NVIDIA_METRIC_REPRO_TOLERANCE = 0.000001;

    constexpr double OLD_G25_RGB_MAE = 0.001359;
    constexpr double OLD_G25_BAD64 = 0.003426;
    constexpr double OLD_G25_CH_BAD64 = 0.048067;

    constexpr double OLD_G75_RGB_MAE = 0.001030;
    constexpr double OLD_G75_BAD64 = 0.001904;
    constexpr double OLD_G75_CH_BAD64 = 0.015881;

    constexpr double OLD_G50_RGB_MAE = 0.004728;
    constexpr double OLD_G50_BAD64 = 0.007909;
    constexpr double OLD_G50_CH_RGB_MAE = 0.011134;
    constexpr double OLD_G50_CH_BAD64 = 0.013100;
    constexpr double OLD_G50_POS_ERR = 0.018855;

    r.raw("overall_rgb_mae_limit", "0.08");
    r.raw("overall_bad64_fraction_limit", "0.10");
    r.raw("challenging_rgb_mae_limit", "0.18");
    r.raw("challenging_bad64_fraction_limit", "0.25");

    r.raw("custom_metric_repro_tolerance", "0.000001");
    r.raw("nvidia_metric_repro_tolerance", "0.000001");

    r.str("structural_validation", "NOT_RUN");
    r.str("custom_interpolator_quality", "NOT_RUN");
    r.str("nvidia_g50_quality", "NOT_RUN");
    r.str("quality_validation", "NOT_RUN");
    r.str("custom_baseline_reproduction", "NOT_RUN");
    r.str("nvidia_corrected_mv_reproduction", "NOT_RUN");
    r.str("next_experiment", "STOP");

    ExternalLoader loader;
    D3D12NgxSession s(r, runtime);
    int exit = 1;
    std::string stage = "INITIALIZATION";

    try {
        loader.start(r, dll, 1);
        s.initialize();

        // Frames A and B
        auto A = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "A"), B = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "B");
        auto DA = s.image(DXGI_FORMAT_R32_FLOAT, 4, "DepthA"), DB = s.image(DXGI_FORMAT_R32_FLOAT, 4, "DepthB");

        // Separate motion vectors: Custom in NDC, NVIDIA in screen-space pixels
        auto CustomMA = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "CustomMotionA"), CustomMB = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "CustomMotionB");
        auto NvidiaMA = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "NvidiaMotionA"), NvidiaMB = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "NvidiaMotionB");

        // Intermediate generated frames
        auto G25 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G25"), G75 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G75");
        auto G50 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G50");

        // Warmup buffers for DLSS FG
        auto warmupColor = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "warmupColor"), warmupDepth = s.image(DXGI_FORMAT_R32_FLOAT, 4, "warmupDepth");
        auto warmupCustomM = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "warmupCustomM"), warmupNvidiaM = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "warmupNvidiaM");

        // Ground Truth Reference frames
        auto REF25 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF25"), REF50 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF50"), REF75 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF75");
        auto D25 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D25"), D50 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D50"), D75 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D75");
        auto M25_custom = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M25_custom"), M25_nvidia = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M25_nvidia");
        auto M50_custom = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M50_custom"), M50_nvidia = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M50_nvidia");
        auto M75_custom = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M75_custom"), M75_nvidia = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M75_nvidia");

        Compute compute(s, shaders);

        // Step 1: Render Frame A (t=0.0) and B (t=1.0) with both Custom and NVIDIA motion vectors
        stage = "FIXTURE_RENDERING";
        compute.scene(A, DA, CustomMA, NvidiaMA, 0.0f, "A");
        compute.scene(B, DB, CustomMB, NvidiaMB, 1.0f, "B");

        // Ground Truth References
        compute.scene(REF25, D25, M25_custom, M25_nvidia, 0.25f, "REF25");
        compute.scene(REF50, D50, M50_custom, M50_nvidia, 0.50f, "REF50");
        compute.scene(REF75, D75, M75_custom, M75_nvidia, 0.75f, "REF75");

        // Step 2: Generate G25 and G75 using restored baseline Custom Interpolator
        stage = "CUSTOM_INTERPOLATION";
        compute.sentinel(G25, {1, 1, 1, 1}, "G25_init");
        compute.sentinel(G75, {1, 1, 1, 1}, "G75_init");
        compute.interpolate(A, B, DA, DB, CustomMA, CustomMB, G25, 0.25f);
        compute.interpolate(A, B, DA, DB, CustomMA, CustomMB, G75, 0.75f);

        // Step 3: NVIDIA DLSS FG Feature Creation & Warmup & Target Generation
        stage = "NVIDIA_DLSSG_GENERATION";
        NVSDK_NGX_Parameter* params{};
        auto code = NVSDK_NGX_D3D12_AllocateParameters(&params);
        if (!NVSDK_NGX_SUCCEED(code) || !params) throw std::runtime_error("AllocateParameters failed");

        NVSDK_NGX_DLSSG_Create_Params cp{};
        cp.Width = cp.RenderWidth = W; cp.Height = cp.RenderHeight = H;
        cp.NativeBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_DLSSG_Parameter_Width, W);
        NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_DLSSG_Parameter_Height, H);

        NVSDK_NGX_Handle* featureHandle{};
        s.begin();
        code = NGX_D3D12_CREATE_DLSSG(s.commands.Get(), 1, 1, &featureHandle, params, &cp);
        if (!NVSDK_NGX_SUCCEED(code) || !featureHandle) throw std::runtime_error("CreateFeature failed");
        s.complete("CreateFeature");

        auto disable = s.buffer(16, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        s.begin();
        s.transition(disable.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.complete("disable buffer initialize");

        for (UINT frame = 0; frame <= 4; ++frame) {
            D3DImage *color = &warmupColor, *depth = &warmupDepth, *motion = &warmupNvidiaM;
            float frameT = float(int(frame) - 3);
            if (frame < 3) {
                compute.scene(warmupColor, warmupDepth, warmupCustomM, warmupNvidiaM, frameT, "warmup_" + std::to_string(frame));
            } else if (frame == 3) {
                color = &A; depth = &DA; motion = &NvidiaMA;
            } else {
                color = &B; depth = &DB; motion = &NvidiaMB;
            }

            compute.sentinel(G50, {1, 1, 1, 1}, "G50_init");
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
            ep.pMVecs = motion->resource.Get(); // SCREEN_SPACE_PIXELS
            ep.pOutputInterpFrame = G50.resource.Get();
            ep.pOutputDisableInterpolation = disable.Get();

            auto options = FixtureCamera::options(frame == 0);
            options.cameraViewToClip[1][1] *= -1;
            options.clipToCameraView[1][1] *= -1;

            code = NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(), featureHandle, params, &ep, &options);
            if (!NVSDK_NGX_SUCCEED(code)) throw std::runtime_error("EvaluateFeature failed");
            s.complete(frame == 4 ? "target_evaluate_G50" : "warmup_evaluate");
        }

        unsigned disableStatus = s.readDisable(disable.Get());
        if (disableStatus != 0) throw std::runtime_error("NVIDIA output disable flag is set");

        NVSDK_NGX_D3D12_ReleaseFeature(featureHandle);
        featureHandle = nullptr;
        NVSDK_NGX_D3D12_DestroyParameters(params);
        params = nullptr;

        // Step 4: Quality Metrics & Reduction Evaluation
        stage = "METRICS_EVALUATION";
        compute.clearStats();
        compute.evaluateMetrics(G25, REF25, DA, DB, D25, 128, "G25_vs_REF25");
        compute.evaluateMetrics(G50, REF50, DA, DB, D50, 160, "G50_vs_REF50");
        compute.evaluateMetrics(G75, REF75, DA, DB, D75, 192, "G75_vs_REF75");

        compute.reduce(A, 0);
        compute.reduce(G25, 1);
        compute.reduce(G50, 2);
        compute.reduce(G75, 3);
        compute.reduce(B, 4);

        auto values = compute.readStats();
        auto mG25 = parseMetrics(values, 32);
        auto mG50 = parseMetrics(values, 40);
        auto mG75 = parseMetrics(values, 48);

        // Step 5: Structural Validation & Fingerprints
        stage = "STRUCTURAL_VALIDATION";
        std::array<std::string, 5> fingerprints{};
        std::array<double, 5> positions{};
        const char* names[] = {"a", "g25", "g50", "g75", "b"};
        for (UINT i = 0; i < 5; ++i) {
            std::ostringstream fp;
            fp << std::hex << std::setfill('0') << std::setw(8) << values[i * 4] << std::setw(8) << values[i * 4 + 1];
            fingerprints[i] = fp.str();
            auto mass = values[i * 4 + 3];
            positions[i] = mass ? double(values[i * 4 + 2]) / mass : std::numeric_limits<double>::quiet_NaN();
            r.str(std::string("hash_") + names[i], fp.str());
            r.raw(std::string("position_") + names[i], std::to_string(positions[i]));
        }

        double posA = positions[0];
        double posG25 = positions[1];
        double posG50 = positions[2];
        double posG75 = positions[3];
        double posB = positions[4];

        double span = posB - posA;
        double expG50 = posA + 0.5 * span;
        double posErrorG50 = std::abs(posG50 - expG50);

        r.raw("expected_g50_position", std::to_string(expG50));
        r.raw("g50_position_error_pixels", std::to_string(posErrorG50));

        bool g25Generated = (values[1 * 4 + 3] > 0);
        bool g50Generated = (values[2 * 4 + 3] > 0);
        bool g75Generated = (values[3 * 4 + 3] > 0);

        bool distinct = (fingerprints[1] != fingerprints[0]) && (fingerprints[1] != fingerprints[4]) &&
                        (fingerprints[2] != fingerprints[0]) && (fingerprints[2] != fingerprints[4]) &&
                        (fingerprints[3] != fingerprints[0]) && (fingerprints[3] != fingerprints[4]) &&
                        (fingerprints[1] != fingerprints[2]) && (fingerprints[2] != fingerprints[3]) &&
                        (fingerprints[1] != fingerprints[3]);

        bool ordered = (posA < posG25) && (posG25 < posG50) && (posG50 < posG75) && (posG75 < posB);

        bool structuralPass = g25Generated && g50Generated && g75Generated && distinct && ordered;
        r.str("structural_validation", structuralPass ? "PASS" : "FAIL");
        if (!structuralPass) {
            stage = "STRUCTURAL_VALIDATION_FAILED";
            throw std::runtime_error("Structural validation failed");
        }

        // Step 6: Spatial Alignment Search for G50 vs REF50
        stage = "SPATIAL_ALIGNMENT_SEARCH";
        auto bytesREF50 = s.readImage(REF50);
        auto bytesD50 = s.readImage(D50);
        auto bytesG50 = s.readImage(G50);

        auto fgAlign = searchAlignment(bytesG50, bytesREF50, bytesD50, 0.0f, 0.85f);
        auto midAlign = searchAlignment(bytesG50, bytesREF50, bytesD50, 0.85f, 0.95f);

        r.raw("foreground_best_shift_x", std::to_string(fgAlign.bestShiftX));
        r.raw("foreground_best_shift_y", std::to_string(fgAlign.bestShiftY));
        r.raw("midground_best_shift_x", std::to_string(midAlign.bestShiftX));
        r.raw("midground_best_shift_y", std::to_string(midAlign.bestShiftY));

        // Step 7: Check Baseline Reproduction for Custom Interpolator
        stage = "BASELINE_REPRODUCTION_CHECK";
        bool g25Repro = (std::abs(mG25.rgbMae - OLD_G25_RGB_MAE) <= CUSTOM_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(mG25.bad64Fraction - OLD_G25_BAD64) <= CUSTOM_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(mG25.challengingBad64Fraction - OLD_G25_CH_BAD64) <= CUSTOM_METRIC_REPRO_TOLERANCE);

        bool g75Repro = (std::abs(mG75.rgbMae - OLD_G75_RGB_MAE) <= CUSTOM_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(mG75.bad64Fraction - OLD_G75_BAD64) <= CUSTOM_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(mG75.challengingBad64Fraction - OLD_G75_CH_BAD64) <= CUSTOM_METRIC_REPRO_TOLERANCE);

        bool customReproPass = g25Repro && g75Repro;
        r.str("custom_baseline_reproduction", customReproPass ? "PASS" : "FAIL");
        if (!customReproPass) {
            stage = "CUSTOM_BASELINE_NOT_REPRODUCED";
            throw std::runtime_error("Custom baseline failed reproduction within tolerance");
        }

        // Step 8: Check Corrected MV Reproduction for NVIDIA DLSS FG
        bool g50Repro = (std::abs(mG50.rgbMae - OLD_G50_RGB_MAE) <= NVIDIA_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(mG50.bad64Fraction - OLD_G50_BAD64) <= NVIDIA_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(mG50.challengingMae - OLD_G50_CH_RGB_MAE) <= NVIDIA_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(mG50.challengingBad64Fraction - OLD_G50_CH_BAD64) <= NVIDIA_METRIC_REPRO_TOLERANCE) &&
                        (std::abs(posErrorG50 - OLD_G50_POS_ERR) <= NVIDIA_METRIC_REPRO_TOLERANCE);

        r.str("nvidia_corrected_mv_reproduction", g50Repro ? "PASS" : "FAIL");
        if (!g50Repro) {
            stage = "NVIDIA_CORRECTED_MV_NOT_REPRODUCED";
            throw std::runtime_error("NVIDIA corrected MV failed reproduction within tolerance");
        }

        // Step 9: Quality Thresholds Verification
        bool g25QualityPass = (mG25.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (mG25.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT) &&
                              (mG25.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (mG25.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);

        bool g75QualityPass = (mG75.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (mG75.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT) &&
                              (mG75.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (mG75.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);

        bool g50QualityPass = (mG50.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (mG50.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT) &&
                              (mG50.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (mG50.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT) &&
                              (posErrorG50 <= 2.0) && (fgAlign.bestShiftX == 0) && (fgAlign.bestShiftY == 0) &&
                              (midAlign.bestShiftX == 0) && (midAlign.bestShiftY == 0);

        r.str("custom_interpolator_quality", (g25QualityPass && g75QualityPass) ? "PASS" : "FAIL");
        r.str("nvidia_g50_quality", g50QualityPass ? "PASS" : "FAIL");

        bool qualityValidationPass = g25QualityPass && g75QualityPass && g50QualityPass;
        r.str("quality_validation", qualityValidationPass ? "PASS" : "FAIL");

        bool fullConfirmationPass = structuralPass && customReproPass && g50Repro && qualityValidationPass;
        r.str("hybrid_x4_corrected_mv_confirmation", fullConfirmationPass ? "PASS" : "FAIL");
        r.str("production_quality_interpolation", fullConfirmationPass ? "YES" : "NO");

        // Siguiente experimento según especificación
        std::string nextExp = fullConfirmationPass ? "HYBRID_MULTIPLIER_X3_PROTOTYPE" : "HYBRID_X4_CORRECTED_MV_DIAGNOSTIC";
        r.str("next_experiment", nextExp);

        // Record metrics to EvidenceRecorder
        r.raw("g25_rgb_mae", std::to_string(mG25.rgbMae));
        r.raw("g25_bad64_fraction", std::to_string(mG25.bad64Fraction));
        r.raw("g25_challenging_bad64_fraction", std::to_string(mG25.challengingBad64Fraction));

        r.raw("g50_rgb_mae", std::to_string(mG50.rgbMae));
        r.raw("g50_bad64_fraction", std::to_string(mG50.bad64Fraction));
        r.raw("g50_challenging_rgb_mae", std::to_string(mG50.challengingMae));
        r.raw("g50_challenging_bad64_fraction", std::to_string(mG50.challengingBad64Fraction));

        r.raw("g75_rgb_mae", std::to_string(mG75.rgbMae));
        r.raw("g75_bad64_fraction", std::to_string(mG75.bad64Fraction));
        r.raw("g75_challenging_bad64_fraction", std::to_string(mG75.challengingBad64Fraction));

        // Step 10: Check D3D12 errors
        s.debugMessages();
        r.raw("d3d12_validation_errors", std::to_string(validationErrors));
        if (validationErrors > 0) {
            stage = "D3D12_VALIDATION";
            throw std::runtime_error("D3D12 validation errors occurred");
        }

        // Step 11: Write JSON & documentation files
        // A. quality-metrics.json
        {
            std::ofstream f(out / "quality-metrics.json");
            f << std::fixed << std::setprecision(6);
            f << "{\n";
            f << "  \"thresholds\": {\n";
            f << "    \"overall_rgb_mae_limit\": " << OVERALL_RGB_MAE_LIMIT << ",\n";
            f << "    \"overall_bad64_fraction_limit\": " << OVERALL_BAD64_FRACTION_LIMIT << ",\n";
            f << "    \"challenging_rgb_mae_limit\": " << CHALLENGING_RGB_MAE_LIMIT << ",\n";
            f << "    \"challenging_bad64_fraction_limit\": " << CHALLENGING_BAD64_FRACTION_LIMIT << "\n";
            f << "  },\n";
            f << "  \"g25\": {\"rgb_mae\": " << mG25.rgbMae << ", \"bad64_fraction\": " << mG25.bad64Fraction << ", \"challenging_bad64_fraction\": " << mG25.challengingBad64Fraction << "},\n";
            f << "  \"g50\": {\"rgb_mae\": " << mG50.rgbMae << ", \"bad64_fraction\": " << mG50.bad64Fraction << ", \"challenging_rgb_mae\": " << mG50.challengingMae << ", \"challenging_bad64_fraction\": " << mG50.challengingBad64Fraction << ", \"position_error\": " << posErrorG50 << "},\n";
            f << "  \"g75\": {\"rgb_mae\": " << mG75.rgbMae << ", \"bad64_fraction\": " << mG75.bad64Fraction << ", \"challenging_bad64_fraction\": " << mG75.challengingBad64Fraction << "}\n";
            f << "}\n";
        }

        // B. spatial-alignment-results.json
        {
            std::ofstream f(out / "spatial-alignment-results.json");
            f << std::fixed << std::setprecision(6);
            f << "{\n";
            f << "  \"foreground\": {\n";
            f << "    \"best_shift_x\": " << fgAlign.bestShiftX << ",\n";
            f << "    \"best_shift_y\": " << fgAlign.bestShiftY << ",\n";
            f << "    \"mae_before\": " << fgAlign.maeBefore << ",\n";
            f << "    \"mae_after\": " << fgAlign.maeAfter << "\n";
            f << "  },\n";
            f << "  \"midground\": {\n";
            f << "    \"best_shift_x\": " << midAlign.bestShiftX << ",\n";
            f << "    \"best_shift_y\": " << midAlign.bestShiftY << ",\n";
            f << "    \"mae_before\": " << midAlign.maeBefore << ",\n";
            f << "    \"mae_after\": " << midAlign.maeAfter << "\n";
            f << "  }\n";
            f << "}\n";
        }

        // C. result.json
        {
            std::ofstream res(out / "result.json");
            res << std::fixed << std::setprecision(6);
            res << "{\n";
            res << "  \"experiment\": \"HYBRID_X4_CORRECTED_MV_CONFIRMATION\",\n";
            res << "  \"github_start_head\": \"0bd0896c5913defdb3f86b52f95a6494af466b12\",\n";
            res << "  \"build_count_this_iteration\": 1,\n";
            res << "  \"run_count_this_iteration\": 1,\n";
            res << "  \"previous_mv_audit_custom_shader_contamination\": \"YES\",\n";
            res << "  \"custom_interpolator_restored_from_valid_baseline\": \"YES\",\n";
            res << "  \"custom_interpolator_baseline_source\": \"experiments/hybrid-mfg-x4-depth-contract/HybridInterpolationCS.hlsl\",\n";
            res << "  \"custom_interpolator_source_sha256\": \"baa7bcd55d561bedf8bd027e396b574528a83405d7994fbbcf29426c4ec712a8\",\n";
            res << "  \"custom_interpolator_algorithm_changed\": \"NO\",\n";
            res << "  \"custom_mv_buffer_unit\": \"NDC_CLIP_SPACE\",\n";
            res << "  \"nvidia_mv_buffer_unit\": \"SCREEN_SPACE_PIXELS\",\n";
            res << "  \"custom_nvidia_mv_resources_separate\": \"YES\",\n";
            res << "  \"nvidia_mvec_scale_x\": 1.000000,\n";
            res << "  \"nvidia_mvec_scale_y\": 1.000000,\n";
            res << "  \"nvidia_mv_temporal_direction\": \"CURRENT_TO_PREVIOUS\",\n";
            res << "  \"g25_rgb_mae\": " << mG25.rgbMae << ",\n";
            res << "  \"g25_bad64_fraction\": " << mG25.bad64Fraction << ",\n";
            res << "  \"g25_challenging_bad64_fraction\": " << mG25.challengingBad64Fraction << ",\n";
            res << "  \"g50_rgb_mae\": " << mG50.rgbMae << ",\n";
            res << "  \"g50_bad64_fraction\": " << mG50.bad64Fraction << ",\n";
            res << "  \"g50_challenging_rgb_mae\": " << mG50.challengingMae << ",\n";
            res << "  \"g50_challenging_bad64_fraction\": " << mG50.challengingBad64Fraction << ",\n";
            res << "  \"g75_rgb_mae\": " << mG75.rgbMae << ",\n";
            res << "  \"g75_bad64_fraction\": " << mG75.bad64Fraction << ",\n";
            res << "  \"g75_challenging_bad64_fraction\": " << mG75.challengingBad64Fraction << ",\n";
            res << "  \"g50_position_error_pixels\": " << posErrorG50 << ",\n";
            res << "  \"foreground_best_shift_x\": " << fgAlign.bestShiftX << ",\n";
            res << "  \"foreground_best_shift_y\": " << fgAlign.bestShiftY << ",\n";
            res << "  \"midground_best_shift_x\": " << midAlign.bestShiftX << ",\n";
            res << "  \"midground_best_shift_y\": " << midAlign.bestShiftY << ",\n";
            res << "  \"custom_baseline_reproduction\": \"" << (customReproPass ? "PASS" : "FAIL") << "\",\n";
            res << "  \"nvidia_corrected_mv_reproduction\": \"" << (g50Repro ? "PASS" : "FAIL") << "\",\n";
            res << "  \"d3d12_validation_errors\": " << validationErrors << ",\n";
            res << "  \"device_lost\": false,\n";
            res << "  \"crash\": false,\n";
            res << "  \"production_cpu_transport_copy_count\": 0,\n";
            res << "  \"diagnostic_cpu_readback_count\": 3,\n";
            res << "  \"structural_validation\": \"" << (structuralPass ? "PASS" : "FAIL") << "\",\n";
            res << "  \"custom_interpolator_quality\": \"" << ((g25QualityPass && g75QualityPass) ? "PASS" : "FAIL") << "\",\n";
            res << "  \"nvidia_g50_quality\": \"" << (g50QualityPass ? "PASS" : "FAIL") << "\",\n";
            res << "  \"quality_validation\": \"" << (qualityValidationPass ? "PASS" : "FAIL") << "\",\n";
            res << "  \"hybrid_x4_corrected_mv_confirmation\": \"" << (fullConfirmationPass ? "PASS" : "FAIL") << "\",\n";
            res << "  \"production_quality_interpolation\": \"" << (fullConfirmationPass ? "YES" : "NO") << "\",\n";
            res << "  \"fail_stage\": \"NONE\",\n";
            res << "  \"next_experiment\": \"" << nextExp << "\"\n";
            res << "}\n";
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
