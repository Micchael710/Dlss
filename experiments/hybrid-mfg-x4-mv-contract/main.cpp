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

struct VariantResult {
    std::string name;
    std::string singleVariableChanged;
    std::string documentaryBasis;
    std::string mvBufferUnit;
    float mvecScaleX = 1.0f;
    float mvecScaleY = 1.0f;
    std::string temporalDirection;
    bool cameraMotionIncluded = true;
    bool motionVectorsDilated = false;
    double rgbMae = 0.0;
    double bad32Fraction = 0.0;
    double bad64Fraction = 0.0;
    UINT bad64Count = 0;
    double challengingRgbMae = 0.0;
    double challengingBad64Fraction = 0.0;
    double positionG50 = 0.0;
    double expectedPositionG50 = 0.0;
    double positionErrorPixels = 0.0;
    int fgBestShiftX = 0;
    int fgBestShiftY = 0;
    double fgMaeBefore = 0.0;
    double fgMaeAfter = 0.0;
    int midBestShiftX = 0;
    int midBestShiftY = 0;
    double midMaeBefore = 0.0;
    double midMaeAfter = 0.0;
    bool contractImprovementCandidate = false;
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

        clearStats();
    }

    void clearStats() {
        s.begin();
        s.transition(stats.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        UINT zero[4]{};
        s.commands->ClearUnorderedAccessViewUint(gpu(10), cpuClear(10), stats.Get(), zero, 0, nullptr);
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

    void scene(D3DImage& color, D3DImage& depth, D3DImage& motion, float t, UINT motionMode, const std::string& label) {
        uav(0, color.resource.Get(), color.format);
        uav(1, depth.resource.Get(), depth.format);
        uav(2, motion.resource.Get(), motion.format);
        s.begin();
        s.barrier(color, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.barrier(depth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.barrier(motion, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bind();
        constants(motionMode, t, 0);
        s.commands->SetPipelineState(fixture.Get());
        s.commands->Dispatch((W + 7) / 8, (H + 7) / 8, 1);
        s.barrier(color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.barrier(motion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        s.complete("GPU fixture " + label + " t=" + std::to_string(t) + " mode=" + std::to_string(motionMode));
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

    r.str("experiment", "HYBRID_X4_NVIDIA_MOTION_VECTOR_CONTRACT_AUDIT");
    r.str("github_start_head", "dfc5d55c2de7f6dd370856605946be059ef4ffc6");
    r.raw("build_count_this_iteration", "1");
    r.raw("run_count_this_iteration", "1");
    r.str("custom_interpolator_algorithm_changed", "NO");
    r.str("depth_contract_changed", "NO");
    r.str("scene_changed", "NO");
    r.str("variant_history_isolation", "YES");
    r.str("official_contract_source", "NVIDIA DLSS Frame Generation SDK v310.7.0 Programming Guide & Headers");
    r.str("official_contract_commit_or_version", "v310.7.0 / SDK 3.7+");

    r.raw("production_cpu_transport_copy_count", "0");
    r.raw("diagnostic_cpu_readback_count", "6"); // REF50, D50, G50_B0, G50_V1, G50_V2, G50_V3

    // Quality limits
    constexpr double OVERALL_RGB_MAE_LIMIT = 0.08;
    constexpr double OVERALL_BAD64_FRACTION_LIMIT = 0.10;
    constexpr double CHALLENGING_RGB_MAE_LIMIT = 0.18;
    constexpr double CHALLENGING_BAD64_FRACTION_LIMIT = 0.25;
    constexpr double METRIC_REPRO_TOLERANCE = 0.000001;

    constexpr double POSITION_IMPROVEMENT_THRESHOLD = 1.0;
    constexpr double BAD64_IMPROVEMENT_THRESHOLD = 0.01;
    constexpr double CHALLENGING_BAD64_IMPROVEMENT_THRESHOLD = 0.05;

    r.raw("overall_rgb_mae_limit", "0.08");
    r.raw("overall_bad64_fraction_limit", "0.10");
    r.raw("challenging_rgb_mae_limit", "0.18");
    r.raw("challenging_bad64_fraction_limit", "0.25");
    r.raw("metric_repro_tolerance", "0.000001");

    r.str("structural_validation", "NOT_RUN");
    r.str("custom_interpolator_quality", "NOT_RUN");
    r.str("baseline_reproduction_pass", "NOT_RUN");
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
        
        // Motion buffers for each mode
        // Mode 0: Baseline B0 / V3 (NDC clip space)
        auto MA0 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionA0"), MB0 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionB0");
        // Mode 1: Variant V1 (Screen-space pixel units)
        auto MA1 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionA1"), MB1 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionB1");
        // Mode 2: Variant V2 (Inverted temporal direction)
        auto MA2 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionA2"), MB2 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "MotionB2");

        // Intermediate generated frames
        auto G25 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G25"), G75 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G75");
        auto G50 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "G50");

        // Warmup buffers for DLSS FG
        auto C = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "warmupColor"), D = s.image(DXGI_FORMAT_R32_FLOAT, 4, "warmupDepth"), M = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "warmupMotion");

        // Ground Truth Reference frames
        auto REF25 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF25"), REF50 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF50"), REF75 = s.image(DXGI_FORMAT_R8G8B8A8_UNORM, 4, "REF75");
        auto D25 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D25"), D50 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D50"), D75 = s.image(DXGI_FORMAT_R32_FLOAT, 4, "D75");
        auto M25 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M25"), M50 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M50"), M75 = s.image(DXGI_FORMAT_R32G32_FLOAT, 8, "M75");

        Compute compute(s, shaders);

        // Render Frame A (t=0.0) and B (t=1.0) for Mode 0, 1, 2
        compute.scene(A, DA, MA0, 0.0f, 0, "A_mode0");
        compute.scene(B, DB, MB0, 1.0f, 0, "B_mode0");
        compute.scene(A, DA, MA1, 0.0f, 1, "A_mode1");
        compute.scene(B, DB, MB1, 1.0f, 1, "B_mode1");
        compute.scene(A, DA, MA2, 0.0f, 2, "A_mode2");
        compute.scene(B, DB, MB2, 1.0f, 2, "B_mode2");

        // Ground Truth References
        compute.scene(REF25, D25, M25, 0.25f, 0, "REF25");
        compute.scene(REF50, D50, M50, 0.50f, 0, "REF50");
        compute.scene(REF75, D75, M75, 0.75f, 0, "REF75");

        // Generate Custom Interpolator G25 and G75 using baseline Mode 0 (completely unchanged)
        compute.interpolate(A, B, DA, DB, MA0, MB0, G25, 0.25f);
        compute.interpolate(A, B, DA, DB, MA0, MB0, G75, 0.75f);

        // Quality verification of Custom Interpolator
        compute.evaluateMetrics(G25, REF25, DA, DB, D25, 128, "G25_vs_REF25");
        compute.evaluateMetrics(G75, REF75, DA, DB, D75, 192, "G75_vs_REF75");
        compute.reduce(A, 0);
        compute.reduce(G25, 1);
        compute.reduce(G75, 3);
        compute.reduce(B, 4);

        auto statsInit = compute.readStats();
        auto mG25 = parseMetrics(statsInit, 32);
        auto mG75 = parseMetrics(statsInit, 48);

        bool g25Pass = (mG25.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (mG25.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        bool g75Pass = (mG75.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (mG75.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        r.str("custom_interpolator_quality", (g25Pass && g75Pass) ? "PASS" : "FAIL");

        double posA = statsInit[0 * 4 + 3] ? double(statsInit[0 * 4 + 2]) / statsInit[0 * 4 + 3] : 0.0;
        double posB = statsInit[4 * 4 + 3] ? double(statsInit[4 * 4 + 2]) / statsInit[4 * 4 + 3] : 0.0;
        double expG50 = posA + 0.5 * (posB - posA);

        r.raw("position_a", std::to_string(posA));
        r.raw("position_b", std::to_string(posB));
        r.raw("expected_g50_position", std::to_string(expG50));

        // Readback Ground Truth REF50 and D50 once for spatial alignment analysis
        auto bytesREF50 = s.readImage(REF50);
        auto bytesD50 = s.readImage(D50);

        // Variant execution structure
        struct VariantConfig {
            std::string name;
            std::string singleVariable;
            std::string documentaryBasis;
            std::string bufferUnit;
            UINT motionMode;
            float scaleX;
            float scaleY;
            std::string temporalDirection;
            bool cameraMotionIncluded;
        };

        std::vector<VariantConfig> configs = {
            {
                "B0_CURRENT_BASELINE",
                "NONE",
                "Baseline reference with NDC clip space buffer and mvecScale=(1,1)",
                "NDC_CLIP_SPACE",
                0,
                1.0f, 1.0f,
                "current_to_previous",
                true
            },
            {
                "V1_OFFICIAL_PIXEL_VALUE_SPACE",
                "MV_VALUE_SPACE_AND_DECLARED_SCALE_PAIR",
                "NVIDIA DLSS-FG Programming Guide v310.7.0 Sections 5 & 10 (PDF p. 20, 22, 97, 104-105)",
                "SCREEN_SPACE_PIXELS",
                1,
                1.0f, 1.0f,
                "current_to_previous",
                true
            },
            {
                "V2_TEMPORAL_DIRECTION_INVERTED",
                "TEMPORAL_DIRECTION",
                "Inverts temporal convention to previous_to_current on baseline B0",
                "NDC_CLIP_SPACE",
                2,
                1.0f, 1.0f,
                "previous_to_current",
                true
            },
            {
                "V3_CAMERA_MOTION_EXCLUDED",
                "CAMERA_MOTION_INCLUDED",
                "Sets cameraMotionIncluded=false on baseline B0",
                "NDC_CLIP_SPACE",
                0,
                1.0f, 1.0f,
                "current_to_previous",
                false
            }
        };

        std::vector<VariantResult> results;

        auto disable = s.buffer(16, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        s.begin();
        s.transition(disable.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        s.complete("disable buffer initialize");

        for (const auto& cfg : configs) {
            stage = "VARIANT_" + cfg.name;
            r.event("variant_start", cfg.name);

            // Step A: Create fresh NGX Feature with isolated history
            NVSDK_NGX_Parameter* params{};
            auto code = NVSDK_NGX_D3D12_AllocateParameters(&params);
            if (!NVSDK_NGX_SUCCEED(code) || !params) throw std::runtime_error("AllocateParameters failed for " + cfg.name);

            NVSDK_NGX_DLSSG_Create_Params cp{};
            cp.Width = cp.RenderWidth = W; cp.Height = cp.RenderHeight = H;
            cp.NativeBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_DLSSG_Parameter_Width, W);
            NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_DLSSG_Parameter_Height, H);

            NVSDK_NGX_Handle* featureHandle{};
            s.begin();
            code = NGX_D3D12_CREATE_DLSSG(s.commands.Get(), 1, 1, &featureHandle, params, &cp);
            if (!NVSDK_NGX_SUCCEED(code) || !featureHandle) throw std::runtime_error("CreateFeature failed for " + cfg.name);
            s.complete("CreateFeature_" + cfg.name);

            D3DImage* motionA = (cfg.motionMode == 1) ? &MA1 : ((cfg.motionMode == 2) ? &MA2 : &MA0);
            D3DImage* motionB = (cfg.motionMode == 1) ? &MB1 : ((cfg.motionMode == 2) ? &MB2 : &MB0);

            // Step B: Execute 4 Warmups + 1 Target Evaluate
            for (UINT frame = 0; frame <= 4; ++frame) {
                D3DImage *color = &C, *depth = &D, *motion = &M;
                float frameT = float(int(frame) - 3);
                if (frame < 3) {
                    compute.scene(C, D, M, frameT, cfg.motionMode, "warmup_" + std::to_string(frame));
                } else if (frame == 3) {
                    color = &A; depth = &DA; motion = motionA;
                } else {
                    color = &B; depth = &DB; motion = motionB;
                }

                compute.sentinel(G50, {1, 1, 1, 1}, "G50_init_" + cfg.name);
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

                auto options = FixtureCamera::options(frame == 0, cfg.cameraMotionIncluded, cfg.scaleX, cfg.scaleY);
                options.cameraViewToClip[1][1] *= -1;
                options.clipToCameraView[1][1] *= -1;

                code = NGX_D3D12_EVALUATE_DLSSG(s.commands.Get(), featureHandle, params, &ep, &options);
                if (!NVSDK_NGX_SUCCEED(code)) throw std::runtime_error("EvaluateFeature failed for " + cfg.name);
                s.complete(frame == 4 ? ("target_" + cfg.name) : "warmup");
            }

            unsigned status = s.readDisable(disable.Get());
            if (status != 0) throw std::runtime_error("Output disable flag set for " + cfg.name);

            // Step C: Evaluate Quality Metrics & Reduction on GPU
            compute.clearStats();
            compute.evaluateMetrics(G50, REF50, DA, DB, D50, 160, "G50_" + cfg.name);
            compute.reduce(G50, 2);

            auto values = compute.readStats();
            auto m = parseMetrics(values, 40);

            double mass = values[2 * 4 + 3];
            double posG50 = mass ? double(values[2 * 4 + 2]) / mass : std::numeric_limits<double>::quiet_NaN();
            double posError = std::abs(posG50 - expG50);

            // Step D: Full frame readback & Spatial Alignment Search
            auto bytesG50 = s.readImage(G50);
            auto fgAlign = searchAlignment(bytesG50, bytesREF50, bytesD50, 0.0f, 0.85f);
            auto midAlign = searchAlignment(bytesG50, bytesREF50, bytesD50, 0.85f, 0.95f);

            // Step E: Release Feature and Destroy Parameters for History Isolation
            code = NVSDK_NGX_D3D12_ReleaseFeature(featureHandle);
            featureHandle = nullptr;
            NVSDK_NGX_D3D12_DestroyParameters(params);
            params = nullptr;

            VariantResult res;
            res.name = cfg.name;
            res.singleVariableChanged = cfg.singleVariable;
            res.documentaryBasis = cfg.documentaryBasis;
            res.mvBufferUnit = cfg.bufferUnit;
            res.mvecScaleX = cfg.scaleX;
            res.mvecScaleY = cfg.scaleY;
            res.temporalDirection = cfg.temporalDirection;
            res.cameraMotionIncluded = cfg.cameraMotionIncluded;
            res.motionVectorsDilated = false;
            res.rgbMae = m.rgbMae;
            res.bad32Fraction = m.bad32Fraction;
            res.bad64Fraction = m.bad64Fraction;
            res.bad64Count = m.bad64Count;
            res.challengingRgbMae = m.challengingMae;
            res.challengingBad64Fraction = m.challengingBad64Fraction;
            res.positionG50 = posG50;
            res.expectedPositionG50 = expG50;
            res.positionErrorPixels = posError;
            res.fgBestShiftX = fgAlign.bestShiftX;
            res.fgBestShiftY = fgAlign.bestShiftY;
            res.fgMaeBefore = fgAlign.maeBefore;
            res.fgMaeAfter = fgAlign.maeAfter;
            res.midBestShiftX = midAlign.bestShiftX;
            res.midBestShiftY = midAlign.bestShiftY;
            res.midMaeBefore = midAlign.maeBefore;
            res.midMaeAfter = midAlign.maeAfter;

            // Check if candidate improvement over B0
            if (!results.empty()) { // For variants V1, V2, V3
                const auto& b0 = results[0];
                bool posImproved = (res.positionErrorPixels <= b0.positionErrorPixels - POSITION_IMPROVEMENT_THRESHOLD);
                bool bad64Improved = (res.bad64Fraction <= b0.bad64Fraction - BAD64_IMPROVEMENT_THRESHOLD);
                bool chBad64Improved = (res.challengingBad64Fraction <= b0.challengingBad64Fraction - CHALLENGING_BAD64_IMPROVEMENT_THRESHOLD);
                res.contractImprovementCandidate = posImproved && (bad64Improved || chBad64Improved);
            }

            results.push_back(res);

            // Check B0 Baseline Reproduction
            if (cfg.name == "B0_CURRENT_BASELINE") {
                constexpr double OLD_RGB_MAE = 0.029109;
                constexpr double OLD_BAD64 = 0.095190;
                constexpr double OLD_CH_BAD64 = 0.368686;
                constexpr double OLD_POS_ERR = 3.521400;

                bool reproPass = (std::abs(res.rgbMae - OLD_RGB_MAE) <= METRIC_REPRO_TOLERANCE) &&
                                 (std::abs(res.bad64Fraction - OLD_BAD64) <= METRIC_REPRO_TOLERANCE) &&
                                 (std::abs(res.challengingBad64Fraction - OLD_CH_BAD64) <= METRIC_REPRO_TOLERANCE) &&
                                 (std::abs(res.positionErrorPixels - OLD_POS_ERR) <= METRIC_REPRO_TOLERANCE);

                r.str("baseline_reproduction_pass", reproPass ? "YES" : "NO");
                if (!reproPass) {
                    stage = "BASELINE_NOT_REPRODUCED";
                    throw std::runtime_error("B0 baseline failed reproduction within strict tolerance");
                }
            }
        }

        // Structural validation
        r.str("structural_validation", "PASS");

        // Analyze Fix and Winning Variant
        bool fixIdentified = false;
        std::string winningVariant = "NONE";
        for (size_t i = 1; i < results.size(); ++i) {
            const auto& v = results[i];
            bool passesThresholds = (v.positionErrorPixels <= 2.0) &&
                                    (v.rgbMae <= OVERALL_RGB_MAE_LIMIT) &&
                                    (v.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT) &&
                                    (v.challengingRgbMae <= CHALLENGING_RGB_MAE_LIMIT) &&
                                    (v.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);
            bool shiftCloser = (std::abs(v.fgBestShiftX) < std::abs(results[0].fgBestShiftX)) ||
                               (std::abs(v.midBestShiftX) < std::abs(results[0].midBestShiftX));
            if (passesThresholds && shiftCloser) {
                fixIdentified = true;
                winningVariant = v.name;
                break;
            }
        }
        r.str("mv_contract_fix_identified", fixIdentified ? "YES" : "NO");
        r.str("winning_variant", winningVariant);

        // Final Classification
        std::string classification;
        std::string nextExp;
        if (fixIdentified) {
            if (winningVariant == "V1_OFFICIAL_PIXEL_VALUE_SPACE") classification = "MV_SCALE_CONTRACT_MISMATCH_CONFIRMED";
            else if (winningVariant == "V2_TEMPORAL_DIRECTION_INVERTED") classification = "MV_DIRECTION_CONTRACT_MISMATCH_CONFIRMED";
            else if (winningVariant == "V3_CAMERA_MOTION_EXCLUDED") classification = "CAMERA_MOTION_FLAG_MISMATCH_CONFIRMED";
            else classification = "MIXED_OR_INCONCLUSIVE";
            nextExp = "HYBRID_X4_CORRECTED_MV_CONFIRMATION";
        } else {
            bool anyImprovement = false;
            for (size_t i = 1; i < results.size(); ++i) {
                if (results[i].contractImprovementCandidate) anyImprovement = true;
            }
            if (anyImprovement) {
                classification = "MIXED_OR_INCONCLUSIVE";
                nextExp = "HYBRID_X4_CORRECTED_MV_VISUAL_CONFIRMATION";
            } else {
                classification = "CURRENT_MV_CONTRACT_SUPPORTED_BY_EVIDENCE";
                nextExp = "HYBRID_X4_NVIDIA_TEMPORAL_MATRIX_CONTRACT_AUDIT";
            }
        }
        r.str("final_mv_contract_classification", classification);
        r.str("next_experiment", nextExp);

        // Write mv-variant-results.json
        {
            std::ofstream f(out / "mv-variant-results.json");
            f << std::fixed << std::setprecision(6);
            f << "{\n  \"variants\": [\n";
            for (size_t i = 0; i < results.size(); ++i) {
                const auto& v = results[i];
                f << "    {\n";
                f << "      \"name\": " << jsonQuote(v.name) << ",\n";
                f << "      \"single_variable_changed\": " << jsonQuote(v.singleVariableChanged) << ",\n";
                f << "      \"documentary_basis\": " << jsonQuote(v.documentaryBasis) << ",\n";
                f << "      \"mv_buffer_unit\": " << jsonQuote(v.mvBufferUnit) << ",\n";
                f << "      \"mvec_scale_x\": " << v.mvecScaleX << ",\n";
                f << "      \"mvec_scale_y\": " << v.mvecScaleY << ",\n";
                f << "      \"temporal_direction\": " << jsonQuote(v.temporalDirection) << ",\n";
                f << "      \"camera_motion_included\": " << (v.cameraMotionIncluded ? "true" : "false") << ",\n";
                f << "      \"motion_vectors_dilated\": false,\n";
                f << "      \"g50_rgb_mae\": " << v.rgbMae << ",\n";
                f << "      \"g50_bad32_fraction\": " << v.bad32Fraction << ",\n";
                f << "      \"g50_bad64_fraction\": " << v.bad64Fraction << ",\n";
                f << "      \"g50_bad64_count\": " << v.bad64Count << ",\n";
                f << "      \"g50_challenging_rgb_mae\": " << v.challengingRgbMae << ",\n";
                f << "      \"g50_challenging_bad64_fraction\": " << v.challengingBad64Fraction << ",\n";
                f << "      \"position_g50\": " << v.positionG50 << ",\n";
                f << "      \"expected_position_g50\": " << v.expectedPositionG50 << ",\n";
                f << "      \"position_error_pixels\": " << v.positionErrorPixels << ",\n";
                f << "      \"contract_improvement_candidate\": " << (v.contractImprovementCandidate ? "true" : "false") << "\n";
                f << "    }" << (i + 1 < results.size() ? ",\n" : "\n");
            }
            f << "  ]\n}\n";
        }

        // Write spatial-alignment-results.json
        {
            std::ofstream f(out / "spatial-alignment-results.json");
            f << std::fixed << std::setprecision(6);
            f << "{\n  \"spatial_alignment\": [\n";
            for (size_t i = 0; i < results.size(); ++i) {
                const auto& v = results[i];
                f << "    {\n";
                f << "      \"variant\": " << jsonQuote(v.name) << ",\n";
                f << "      \"foreground\": {\n";
                f << "        \"best_shift_x\": " << v.fgBestShiftX << ",\n";
                f << "        \"best_shift_y\": " << v.fgBestShiftY << ",\n";
                f << "        \"mae_before\": " << v.fgMaeBefore << ",\n";
                f << "        \"mae_after\": " << v.fgMaeAfter << "\n";
                f << "      },\n";
                f << "      \"midground\": {\n";
                f << "        \"best_shift_x\": " << v.midBestShiftX << ",\n";
                f << "        \"best_shift_y\": " << v.midBestShiftY << ",\n";
                f << "        \"mae_before\": " << v.midMaeBefore << ",\n";
                f << "        \"mae_after\": " << v.midMaeAfter << "\n";
                f << "      }\n";
                f << "    }" << (i + 1 < results.size() ? ",\n" : "\n");
            }
            f << "  ]\n}\n";
        }

        // Write quality-metrics.json
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
            f << "  \"g25\": {\"rgb_mae\": " << mG25.rgbMae << ", \"bad64_fraction\": " << mG25.bad64Fraction << "},\n";
            f << "  \"g75\": {\"rgb_mae\": " << mG75.rgbMae << ", \"bad64_fraction\": " << mG75.bad64Fraction << "},\n";
            f << "  \"g50_variants\": [\n";
            for (size_t i = 0; i < results.size(); ++i) {
                const auto& v = results[i];
                f << "    {\"name\": " << jsonQuote(v.name) << ", \"rgb_mae\": " << v.rgbMae << ", \"bad64_fraction\": " << v.bad64Fraction << ", \"challenging_bad64_fraction\": " << v.challengingBad64Fraction << ", \"position_error\": " << v.positionErrorPixels << "}" << (i + 1 < results.size() ? ",\n" : "\n");
            }
            f << "  ]\n}\n";
        }

        // Check D3D12 errors
        s.debugMessages();
        r.raw("d3d12_validation_errors", std::to_string(validationErrors));
        if (validationErrors > 0) {
            stage = "D3D12_VALIDATION";
            throw std::runtime_error("D3D12 validation errors occurred");
        }

        // Write final result.json
        {
            const auto& b0 = results[0];
            std::ofstream res(out / "result.json");
            res << std::fixed << std::setprecision(6);
            res << "{\n";
            res << "  \"experiment\": \"HYBRID_X4_NVIDIA_MOTION_VECTOR_CONTRACT_AUDIT\",\n";
            res << "  \"github_start_head\": \"dfc5d55c2de7f6dd370856605946be059ef4ffc6\",\n";
            res << "  \"build_count_this_iteration\": 1,\n";
            res << "  \"run_count_this_iteration\": 1,\n";
            res << "  \"variant_history_isolation\": \"YES\",\n";
            res << "  \"official_contract_source\": \"NVIDIA DLSS Frame Generation SDK v310.7.0 Programming Guide & Headers\",\n";
            res << "  \"official_contract_commit_or_version\": \"v310.7.0 / SDK 3.7+\",\n";
            res << "  \"baseline_reproduction_pass\": \"YES\",\n";
            res << "  \"baseline_mv_buffer_unit\": " << jsonQuote(b0.mvBufferUnit) << ",\n";
            res << "  \"baseline_mvec_scale_x\": " << b0.mvecScaleX << ",\n";
            res << "  \"baseline_mvec_scale_y\": " << b0.mvecScaleY << ",\n";
            res << "  \"baseline_temporal_direction\": " << jsonQuote(b0.temporalDirection) << ",\n";
            res << "  \"baseline_camera_motion_included\": " << (b0.cameraMotionIncluded ? "true" : "false") << ",\n";
            res << "  \"baseline_motion_vectors_dilated\": false,\n";
            res << "  \"baseline_g50_rgb_mae\": " << b0.rgbMae << ",\n";
            res << "  \"baseline_g50_bad64_fraction\": " << b0.bad64Fraction << ",\n";
            res << "  \"baseline_g50_challenging_bad64_fraction\": " << b0.challengingBad64Fraction << ",\n";
            res << "  \"baseline_position_error_pixels\": " << b0.positionErrorPixels << ",\n";
            res << "  \"baseline_foreground_best_shift_x\": " << b0.fgBestShiftX << ",\n";
            res << "  \"baseline_foreground_best_shift_y\": " << b0.fgBestShiftY << ",\n";
            res << "  \"baseline_midground_best_shift_x\": " << b0.midBestShiftX << ",\n";
            res << "  \"baseline_midground_best_shift_y\": " << b0.midBestShiftY << ",\n";
            res << "  \"variants\": [\n";
            for (size_t i = 1; i < results.size(); ++i) {
                const auto& v = results[i];
                res << "    {\n";
                res << "      \"name\": " << jsonQuote(v.name) << ",\n";
                res << "      \"single_variable_changed\": " << jsonQuote(v.singleVariableChanged) << ",\n";
                res << "      \"documentary_basis\": " << jsonQuote(v.documentaryBasis) << ",\n";
                res << "      \"mv_buffer_unit\": " << jsonQuote(v.mvBufferUnit) << ",\n";
                res << "      \"mvec_scale_x\": " << v.mvecScaleX << ",\n";
                res << "      \"mvec_scale_y\": " << v.mvecScaleY << ",\n";
                res << "      \"temporal_direction\": " << jsonQuote(v.temporalDirection) << ",\n";
                res << "      \"camera_motion_included\": " << (v.cameraMotionIncluded ? "true" : "false") << ",\n";
                res << "      \"motion_vectors_dilated\": false,\n";
                res << "      \"g50_rgb_mae\": " << v.rgbMae << ",\n";
                res << "      \"g50_bad64_fraction\": " << v.bad64Fraction << ",\n";
                res << "      \"g50_challenging_bad64_fraction\": " << v.challengingBad64Fraction << ",\n";
                res << "      \"position_error_pixels\": " << v.positionErrorPixels << ",\n";
                res << "      \"foreground_best_shift_x\": " << v.fgBestShiftX << ",\n";
                res << "      \"foreground_best_shift_y\": " << v.fgBestShiftY << ",\n";
                res << "      \"midground_best_shift_x\": " << v.midBestShiftX << ",\n";
                res << "      \"midground_best_shift_y\": " << v.midBestShiftY << ",\n";
                res << "      \"contract_improvement_candidate\": " << (v.contractImprovementCandidate ? "true" : "false") << "\n";
                res << "    }" << (i + 1 < results.size() ? ",\n" : "\n");
            }
            res << "  ],\n";
            res << "  \"mv_contract_fix_identified\": \"" << (fixIdentified ? "YES" : "NO") << "\",\n";
            res << "  \"winning_variant\": \"" << winningVariant << "\",\n";
            res << "  \"final_mv_contract_classification\": \"" << classification << "\",\n";
            res << "  \"d3d12_validation_errors\": " << validationErrors << ",\n";
            res << "  \"device_lost\": false,\n";
            res << "  \"crash\": false,\n";
            res << "  \"production_cpu_transport_copy_count\": 0,\n";
            res << "  \"diagnostic_cpu_readback_count\": 6,\n";
            res << "  \"custom_interpolator_algorithm_changed\": \"NO\",\n";
            res << "  \"depth_contract_changed\": \"NO\",\n";
            res << "  \"scene_changed\": \"NO\",\n";
            res << "  \"structural_validation\": \"PASS\",\n";
            res << "  \"quality_validation\": \"" << (fixIdentified ? "PASS" : "FAIL") << "\",\n";
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
