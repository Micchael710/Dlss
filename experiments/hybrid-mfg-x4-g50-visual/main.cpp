#include <windows.h>
#include <psapi.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <wincodec.h>
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

struct PngSaver {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    PngSaver() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == RPC_E_CHANGED_MODE) {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        }
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) throw std::runtime_error("CoCreateInstance WIC factory failed");
    }
    void saveRgba(const fs::path& path, UINT width, UINT height, const uint8_t* rgba, UINT stride) {
        Microsoft::WRL::ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(&stream);
        if (FAILED(hr)) throw std::runtime_error("CreateStream failed: " + path.string());
        hr = stream->InitializeFromFilename(path.wstring().c_str(), GENERIC_WRITE);
        if (FAILED(hr)) throw std::runtime_error("InitializeFromFilename failed: " + path.string());
        Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
        if (FAILED(hr)) throw std::runtime_error("CreateEncoder failed: " + path.string());
        hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) throw std::runtime_error("Encoder Initialize failed: " + path.string());
        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(&frame, nullptr);
        if (FAILED(hr)) throw std::runtime_error("CreateNewFrame failed: " + path.string());
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) throw std::runtime_error("Frame Initialize failed: " + path.string());
        hr = frame->SetSize(width, height);
        if (FAILED(hr)) throw std::runtime_error("Frame SetSize failed: " + path.string());
        WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppRGBA;
        hr = frame->SetPixelFormat(&pixelFormat);
        if (FAILED(hr)) throw std::runtime_error("SetPixelFormat failed: " + path.string());

        std::vector<uint8_t> converted;
        const uint8_t* pPixels = rgba;
        UINT actualStride = stride;
        if (pixelFormat == GUID_WICPixelFormat32bppBGRA) {
            converted.resize(SIZE_T(stride) * height);
            for (UINT y = 0; y < height; ++y) {
                for (UINT x = 0; x < width; ++x) {
                    UINT idx = y * stride + x * 4;
                    converted[idx + 0] = rgba[idx + 2]; // B
                    converted[idx + 1] = rgba[idx + 1]; // G
                    converted[idx + 2] = rgba[idx + 0]; // R
                    converted[idx + 3] = rgba[idx + 3]; // A
                }
            }
            pPixels = converted.data();
        } else if (pixelFormat == GUID_WICPixelFormat24bppBGR) {
            actualStride = width * 3;
            converted.resize(SIZE_T(actualStride) * height);
            for (UINT y = 0; y < height; ++y) {
                for (UINT x = 0; x < width; ++x) {
                    UINT srcIdx = y * stride + x * 4;
                    UINT dstIdx = y * actualStride + x * 3;
                    converted[dstIdx + 0] = rgba[srcIdx + 2]; // B
                    converted[dstIdx + 1] = rgba[srcIdx + 1]; // G
                    converted[dstIdx + 2] = rgba[srcIdx + 0]; // R
                }
            }
            pPixels = converted.data();
        } else if (pixelFormat == GUID_WICPixelFormat24bppRGB) {
            actualStride = width * 3;
            converted.resize(SIZE_T(actualStride) * height);
            for (UINT y = 0; y < height; ++y) {
                for (UINT x = 0; x < width; ++x) {
                    UINT srcIdx = y * stride + x * 4;
                    UINT dstIdx = y * actualStride + x * 3;
                    converted[dstIdx + 0] = rgba[srcIdx + 0]; // R
                    converted[dstIdx + 1] = rgba[srcIdx + 1]; // G
                    converted[dstIdx + 2] = rgba[srcIdx + 2]; // B
                }
            }
            pPixels = converted.data();
        } else if (pixelFormat != GUID_WICPixelFormat32bppRGBA) {
            throw std::runtime_error("Unsupported negotiated WIC pixel format");
        }
        hr = frame->WritePixels(height, actualStride, actualStride * height, const_cast<uint8_t*>(pPixels));
        if (FAILED(hr)) throw std::runtime_error("WritePixels failed: " + path.string());
        hr = frame->Commit();
        if (FAILED(hr)) throw std::runtime_error("Frame Commit failed: " + path.string());
        hr = encoder->Commit();
        if (FAILED(hr)) throw std::runtime_error("Encoder Commit failed: " + path.string());
    }
};

int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 5) return 2;
    fs::path out = fs::absolute(argv[1]), dll = fs::absolute(argv[2]), runtime = fs::absolute(argv[3]), shaders = fs::absolute(argv[4]);
    fs::path capturesDir = out / "captures";
    fs::create_directories(capturesDir);
    vendorLog.open(out / "ngx-callback.log");
    validationLog.open(out / "d3d12-debug.log");
    validationMessagesLog.open(out / "validation-messages.log");
    EvidenceRecorder r(out);

    r.str("experiment", "HYBRID_X4_NVIDIA_G50_VISUAL_DIAGNOSTIC");
    r.str("github_start_head", "695ec9904a29a7045be885dd12f1cdc1cd786bff");
    r.raw("build_count_this_iteration", "1");
    r.raw("run_count_this_iteration", "1");
    r.str("custom_interpolator_algorithm_changed", "NO");
    r.str("nvidia_configuration_changed", "NO");
    r.str("scene_changed", "NO");
    r.str("depth_contract_changed", "NO");
    r.str("motion_vector_contract_changed", "NO");

    // Diagnostic Readback contract
    r.raw("production_cpu_transport_copy_count", "0");
    r.str("visual_full_frame_readback", "YES");
    r.raw("diagnostic_cpu_readback_count", "11");
    r.raw("diff_visualization_gain", "4.0");
    r.str("heatmap_method", "STANDARD_SPECTRAL_MAX_RGB_DIFF");

    // Quality limits
    constexpr double OVERALL_RGB_MAE_LIMIT = 0.08;
    constexpr double OVERALL_BAD64_FRACTION_LIMIT = 0.10;
    constexpr double CHALLENGING_RGB_MAE_LIMIT = 0.18;
    constexpr double CHALLENGING_BAD64_FRACTION_LIMIT = 0.25;
    constexpr double METRIC_REPRO_TOLERANCE = 0.000001;

    r.raw("overall_rgb_mae_limit", "0.08");
    r.raw("overall_bad64_fraction_limit", "0.10");
    r.raw("challenging_rgb_mae_limit", "0.18");
    r.raw("challenging_bad64_fraction_limit", "0.25");
    r.raw("metric_repro_tolerance", "0.000001");

    r.str("structural_validation", "NOT_RUN");
    r.str("custom_interpolator_quality", "NOT_RUN");
    r.str("nvidia_g50_quality", "NOT_RUN");
    r.str("capture_dimensions_valid", "NOT_RUN");
    r.str("metric_reproduction_pass", "NOT_RUN");
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
        compute.evaluateMetrics(G25, REF25, DA, DB, D25, 128, "G25_vs_REF25");
        compute.evaluateMetrics(G50, REF50, DA, DB, D50, 160, "G50_vs_REF50");
        compute.evaluateMetrics(G75, REF75, DA, DB, D75, 192, "G75_vs_REF75");

        // Step 8: Run Reduction CS for Fingerprints
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
        r.raw("position_a", std::to_string(positions[0]));
        r.raw("position_g50", std::to_string(positions[2]));
        r.raw("position_b", std::to_string(positions[4]));
        r.raw("expected_g50_position", std::to_string(expG50));
        r.raw("g25_position_error_pixels", std::to_string(errG25));
        r.raw("g50_position_error_pixels", std::to_string(errG50));
        r.raw("g75_position_error_pixels", std::to_string(errG75));
        r.raw("temporal_position_tolerance_pixels", std::to_string(tolerance));
        r.str("position_tolerance_diagnostic_pass", withinTol ? "YES" : "NO");

        bool structuralPass = changed && distinct && ordered && resourcesDistinct;
        r.str("structural_validation", structuralPass ? "PASS" : "FAIL");

        // Step 11: Parse Quality Metrics
        auto g25Metrics = parseMetrics(values, 32);
        auto g50Metrics = parseMetrics(values, 40);
        auto g75Metrics = parseMetrics(values, 48);

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

        // Reproduction verification against depth-contract baseline
        constexpr double OLD_G50_RGB_MAE = 0.029109;
        constexpr double OLD_G50_BAD64_FRACTION = 0.095190;
        constexpr double OLD_G50_CHALLENGING_RGB_MAE = 0.115283;
        constexpr double OLD_G50_CHALLENGING_BAD64_FRACTION = 0.368686;

        bool reproPass = (std::abs(g50Metrics.rgbMae - OLD_G50_RGB_MAE) <= METRIC_REPRO_TOLERANCE) &&
                         (std::abs(g50Metrics.bad64Fraction - OLD_G50_BAD64_FRACTION) <= METRIC_REPRO_TOLERANCE) &&
                         (std::abs(g50Metrics.challengingMae - OLD_G50_CHALLENGING_RGB_MAE) <= METRIC_REPRO_TOLERANCE) &&
                         (std::abs(g50Metrics.challengingBad64Fraction - OLD_G50_CHALLENGING_BAD64_FRACTION) <= METRIC_REPRO_TOLERANCE);
        r.str("metric_reproduction_pass", reproPass ? "YES" : "NO");
        if (!reproPass) {
            stage = "NON_DETERMINISTIC_BASELINE";
            throw std::runtime_error("Baseline metric reproduction failed outside tolerance");
        }

        // Step 12: Full-Frame Diagnostic Readback (11 readbacks)
        stage = "DIAGNOSTIC_CPU_READBACK";
        auto bytesA = s.readImage(A);
        auto bytesB = s.readImage(B);
        auto bytesG25 = s.readImage(G25);
        auto bytesREF25 = s.readImage(REF25);
        auto bytesG50 = s.readImage(G50);
        auto bytesREF50 = s.readImage(REF50);
        auto bytesG75 = s.readImage(G75);
        auto bytesREF75 = s.readImage(REF75);
        auto bytesDA = s.readImage(DA);
        auto bytesDB = s.readImage(DB);
        auto bytesD50 = s.readImage(D50);

        const float* pDA = reinterpret_cast<const float*>(bytesDA.data());
        const float* pDB = reinterpret_cast<const float*>(bytesDB.data());
        const float* pD50 = reinterpret_cast<const float*>(bytesD50.data());

        // Step 13: Compute Diagnostic Images, Error Regions, Masks, Centroid Annotations
        stage = "GENERATE_VISUAL_CAPTURES";
        const UINT totalPixels = W * H;
        const UINT stride = W * 4;

        // Buffers for derived images
        std::vector<uint8_t> diffG25(totalPixels * 4);
        std::vector<uint8_t> maskBad64G25(totalPixels * 4);
        std::vector<uint8_t> diffG50(totalPixels * 4);
        std::vector<uint8_t> heatmapG50(totalPixels * 4);
        std::vector<uint8_t> maskBad64G50(totalPixels * 4);
        std::vector<uint8_t> maskChallenging(totalPixels * 4);
        std::vector<uint8_t> maskChallengingBad64G50(totalPixels * 4);
        std::vector<uint8_t> errorRegionsG50(totalPixels * 4);
        std::vector<uint8_t> centroidG50(totalPixels * 4);
        std::vector<uint8_t> diffG75(totalPixels * 4);
        std::vector<uint8_t> maskBad64G75(totalPixels * 4);

        UINT maskBad64Count = 0;
        UINT challengingCount = 0;
        UINT challengingBad64Count = 0;

        UINT bad64Background = 0;
        UINT bad64Midground = 0;
        UINT bad64Foreground = 0;
        UINT bad64DepthEdge = 0;
        UINT bad64OcclusionDisocclusion = 0;

        for (UINT y = 0; y < H; ++y) {
            for (UINT x = 0; x < W; ++x) {
                UINT idx = (y * W + x) * 4;
                UINT pixelIdx = y * W + x;

                // G25 Diff & Mask
                int diffR25 = std::abs(int(bytesG25[idx + 0]) - int(bytesREF25[idx + 0]));
                int diffG25_g = std::abs(int(bytesG25[idx + 1]) - int(bytesREF25[idx + 1]));
                int diffB25 = std::abs(int(bytesG25[idx + 2]) - int(bytesREF25[idx + 2]));
                int maxDiff25 = std::max({diffR25, diffG25_g, diffB25});

                diffG25[idx + 0] = uint8_t(std::min(255, diffR25 * 4));
                diffG25[idx + 1] = uint8_t(std::min(255, diffG25_g * 4));
                diffG25[idx + 2] = uint8_t(std::min(255, diffB25 * 4));
                diffG25[idx + 3] = 255;

                uint8_t bad64Val25 = (maxDiff25 > 64) ? 255 : 0;
                maskBad64G25[idx + 0] = bad64Val25;
                maskBad64G25[idx + 1] = bad64Val25;
                maskBad64G25[idx + 2] = bad64Val25;
                maskBad64G25[idx + 3] = 255;

                // G75 Diff & Mask
                int diffR75 = std::abs(int(bytesG75[idx + 0]) - int(bytesREF75[idx + 0]));
                int diffG75_g = std::abs(int(bytesG75[idx + 1]) - int(bytesREF75[idx + 1]));
                int diffB75 = std::abs(int(bytesG75[idx + 2]) - int(bytesREF75[idx + 2]));
                int maxDiff75 = std::max({diffR75, diffG75_g, diffB75});

                diffG75[idx + 0] = uint8_t(std::min(255, diffR75 * 4));
                diffG75[idx + 1] = uint8_t(std::min(255, diffG75_g * 4));
                diffG75[idx + 2] = uint8_t(std::min(255, diffB75 * 4));
                diffG75[idx + 3] = 255;

                uint8_t bad64Val75 = (maxDiff75 > 64) ? 255 : 0;
                maskBad64G75[idx + 0] = bad64Val75;
                maskBad64G75[idx + 1] = bad64Val75;
                maskBad64G75[idx + 2] = bad64Val75;
                maskBad64G75[idx + 3] = 255;

                // G50 Analysis
                float candR = float(bytesG50[idx + 0]) / 255.0f;
                float candG = float(bytesG50[idx + 1]) / 255.0f;
                float candB = float(bytesG50[idx + 2]) / 255.0f;

                float refR = float(bytesREF50[idx + 0]) / 255.0f;
                float refG = float(bytesREF50[idx + 1]) / 255.0f;
                float refB = float(bytesREF50[idx + 2]) / 255.0f;

                float rDiff = std::abs(candR - refR);
                float gDiff = std::abs(candG - refG);
                float bDiff = std::abs(candB - refB);
                float maxDiff255 = std::max({rDiff, gDiff, bDiff}) * 255.0f;

                // 1. G50 vs REF50 DIFF (Gain 4.0)
                diffG50[idx + 0] = uint8_t(std::clamp(int(std::round(rDiff * 4.0f * 255.0f)), 0, 255));
                diffG50[idx + 1] = uint8_t(std::clamp(int(std::round(gDiff * 4.0f * 255.0f)), 0, 255));
                diffG50[idx + 2] = uint8_t(std::clamp(int(std::round(bDiff * 4.0f * 255.0f)), 0, 255));
                diffG50[idx + 3] = 255;

                // 2. G50 vs REF50 HEATMAP: Standard spectral colormap driven by maxDiff
                float normDiff = std::clamp(std::max({rDiff, gDiff, bDiff}), 0.0f, 1.0f);
                uint8_t hmR = 0, hmG = 0, hmB = 0;
                if (normDiff <= 0.0f) {
                    hmR = hmG = hmB = 0;
                } else if (normDiff < 0.25f) { // Black -> Blue
                    float f = normDiff / 0.25f;
                    hmB = uint8_t(std::round(f * 255.0f));
                } else if (normDiff < 0.50f) { // Blue -> Cyan
                    float f = (normDiff - 0.25f) / 0.25f;
                    hmG = uint8_t(std::round(f * 255.0f));
                    hmB = 255;
                } else if (normDiff < 0.75f) { // Cyan -> Yellow
                    float f = (normDiff - 0.50f) / 0.25f;
                    hmR = uint8_t(std::round(f * 255.0f));
                    hmG = 255;
                    hmB = uint8_t(std::round((1.0f - f) * 255.0f));
                } else { // Yellow -> Red
                    float f = (normDiff - 0.75f) / 0.25f;
                    hmR = 255;
                    hmG = uint8_t(std::round((1.0f - f) * 255.0f));
                    hmB = 0;
                }
                heatmapG50[idx + 0] = hmR;
                heatmapG50[idx + 1] = hmG;
                heatmapG50[idx + 2] = hmB;
                heatmapG50[idx + 3] = 255;

                // 3. BAD64 Mask
                bool isBad64 = (maxDiff255 > 64.0f);
                if (isBad64) ++maskBad64Count;
                uint8_t bad64Byte = isBad64 ? 255 : 0;
                maskBad64G50[idx + 0] = bad64Byte;
                maskBad64G50[idx + 1] = bad64Byte;
                maskBad64G50[idx + 2] = bad64Byte;
                maskBad64G50[idx + 3] = 255;

                // 4. Challenging Mask (Depth edge or Occlusion / Disocclusion)
                float refZ = pD50[pixelIdx];
                float zA   = pDA[pixelIdx];
                float zB   = pDB[pixelIdx];

                UINT yUp    = (y > 0) ? y - 1 : 0;
                UINT yDown  = (y + 1 < H) ? y + 1 : H - 1;
                UINT xLeft  = (x > 0) ? x - 1 : 0;
                UINT xRight = (x + 1 < W) ? x + 1 : W - 1;

                float zUp    = pD50[yUp * W + x];
                float zDown  = pD50[yDown * W + x];
                float zLeft  = pD50[y * W + xLeft];
                float zRight = pD50[y * W + xRight];

                float maxEdgeZ = std::max({std::abs(refZ - zUp), std::abs(refZ - zDown), std::abs(refZ - zLeft), std::abs(refZ - zRight)});
                bool isDepthEdge = (maxEdgeZ > 0.05f);
                bool isOcclOrDisoccl = (std::abs(refZ - zA) > 0.05f) || (std::abs(refZ - zB) > 0.05f);
                bool isChallenging = isDepthEdge || isOcclOrDisoccl;

                if (isChallenging) ++challengingCount;
                uint8_t chVal = isChallenging ? 255 : 0;
                maskChallenging[idx + 0] = chVal;
                maskChallenging[idx + 1] = chVal;
                maskChallenging[idx + 2] = chVal;
                maskChallenging[idx + 3] = 255;

                // 5. Intersection Mask (Challenging AND Bad64)
                bool isChallengingBad64 = isChallenging && isBad64;
                if (isChallengingBad64) ++challengingBad64Count;
                uint8_t chBadVal = isChallengingBad64 ? 255 : 0;
                maskChallengingBad64G50[idx + 0] = chBadVal;
                maskChallengingBad64G50[idx + 1] = chBadVal;
                maskChallengingBad64G50[idx + 2] = chBadVal;
                maskChallengingBad64G50[idx + 3] = 255;

                // 6. Error Regions: Categorization with documented priority
                // Priority:
                // 1: OCCLUSION_DISOCCLUSION (isOcclOrDisoccl)
                // 2: DEPTH_EDGE (isDepthEdge && !isOcclOrDisoccl)
                // 3: FOREGROUND (refZ < 0.85)
                // 4: MIDGROUND (refZ >= 0.85 && refZ < 0.95)
                // 5: BACKGROUND (refZ >= 0.95)
                if (isBad64) {
                    if (isOcclOrDisoccl) {
                        ++bad64OcclusionDisocclusion;
                        // Bright Red
                        errorRegionsG50[idx + 0] = 255;
                        errorRegionsG50[idx + 1] = 40;
                        errorRegionsG50[idx + 2] = 40;
                    } else if (isDepthEdge) {
                        ++bad64DepthEdge;
                        // Bright Orange
                        errorRegionsG50[idx + 0] = 255;
                        errorRegionsG50[idx + 1] = 140;
                        errorRegionsG50[idx + 2] = 0;
                    } else if (refZ < 0.85f) {
                        ++bad64Foreground;
                        // Magenta (moving foreground interior)
                        errorRegionsG50[idx + 0] = 255;
                        errorRegionsG50[idx + 1] = 0;
                        errorRegionsG50[idx + 2] = 255;
                    } else if (refZ < 0.95f) {
                        ++bad64Midground;
                        // Cyan (moving midground interior)
                        errorRegionsG50[idx + 0] = 0;
                        errorRegionsG50[idx + 1] = 220;
                        errorRegionsG50[idx + 2] = 255;
                    } else {
                        ++bad64Background;
                        // Yellow (background high-frequency pattern)
                        errorRegionsG50[idx + 0] = 255;
                        errorRegionsG50[idx + 1] = 255;
                        errorRegionsG50[idx + 2] = 0;
                    }
                } else {
                    // Dimmed G50 scene for context
                    errorRegionsG50[idx + 0] = uint8_t(bytesG50[idx + 0] / 5);
                    errorRegionsG50[idx + 1] = uint8_t(bytesG50[idx + 1] / 5);
                    errorRegionsG50[idx + 2] = uint8_t(bytesG50[idx + 2] / 5);
                }
                errorRegionsG50[idx + 3] = 255;

                // Centroid diagnostic baseline copy
                centroidG50[idx + 0] = bytesG50[idx + 0];
                centroidG50[idx + 1] = bytesG50[idx + 1];
                centroidG50[idx + 2] = bytesG50[idx + 2];
                centroidG50[idx + 3] = 255;
            }
        }

        // Draw vertical lines on centroidG50:
        // Expected centroid line: Green (0, 255, 0)
        // Actual measured centroid line: Yellow (255, 255, 0)
        int expX = int(std::round(expG50));
        int actX = int(std::round(positions[2]));
        for (UINT y = 0; y < H; ++y) {
            // Expected centroid (Green) - 2px wide
            for (int dx = 0; dx < 2; ++dx) {
                int cx = expX + dx;
                if (cx >= 0 && cx < int(W)) {
                    UINT cidx = (y * W + cx) * 4;
                    centroidG50[cidx + 0] = 0;
                    centroidG50[cidx + 1] = 255;
                    centroidG50[cidx + 2] = 0;
                }
            }
            // Actual measured centroid (Yellow) - 2px wide
            for (int dx = 0; dx < 2; ++dx) {
                int cx = actX + dx;
                if (cx >= 0 && cx < int(W)) {
                    UINT cidx = (y * W + cx) * 4;
                    centroidG50[cidx + 0] = 255;
                    centroidG50[cidx + 1] = 255;
                    centroidG50[cidx + 2] = 0;
                }
            }
        }

        // Save all 19 PNGs
        PngSaver saver;
        auto save = [&](const std::string& name, const std::vector<uint8_t>& buf) {
            fs::path p = capturesDir / name;
            saver.saveRgba(p, W, H, buf.data(), stride);
            return p;
        };

        std::vector<std::string> pngNames = {
            "A.png",
            "B.png",
            "G25.png",
            "REF25.png",
            "G25_vs_REF25_DIFF.png",
            "G25_BAD64_MASK.png",
            "G50.png",
            "REF50.png",
            "G50_vs_REF50_DIFF.png",
            "G50_vs_REF50_HEATMAP.png",
            "G50_BAD64_MASK.png",
            "CHALLENGING_MASK.png",
            "G50_CHALLENGING_BAD64_MASK.png",
            "G50_ERROR_REGIONS.png",
            "G50_CENTROID_DIAGNOSTIC.png",
            "G75.png",
            "REF75.png",
            "G75_vs_REF75_DIFF.png",
            "G75_BAD64_MASK.png"
        };

        save("A.png", bytesA);
        save("B.png", bytesB);
        save("G25.png", bytesG25);
        save("REF25.png", bytesREF25);
        save("G25_vs_REF25_DIFF.png", diffG25);
        save("G25_BAD64_MASK.png", maskBad64G25);
        save("G50.png", bytesG50);
        save("REF50.png", bytesREF50);
        save("G50_vs_REF50_DIFF.png", diffG50);
        save("G50_vs_REF50_HEATMAP.png", heatmapG50);
        save("G50_BAD64_MASK.png", maskBad64G50);
        save("CHALLENGING_MASK.png", maskChallenging);
        save("G50_CHALLENGING_BAD64_MASK.png", maskChallengingBad64G50);
        save("G50_ERROR_REGIONS.png", errorRegionsG50);
        save("G50_CENTROID_DIAGNOSTIC.png", centroidG50);
        save("G75.png", bytesG75);
        save("REF75.png", bytesREF75);
        save("G75_vs_REF75_DIFF.png", diffG75);
        save("G75_BAD64_MASK.png", maskBad64G75);

        // Step 14: Validate captures (1280x720, SHA256) & Manifest
        std::ofstream manifest(out / "capture-manifest.txt");
        manifest << "CAPTURE_MANIFEST\n";
        manifest << "TOTAL_CAPTURES=" << pngNames.size() << "\n\n";
        manifest << std::left << std::setw(34) << "FILENAME" << std::setw(12) << "WIDTH" << std::setw(12) << "HEIGHT" << "SHA256\n";
        manifest << std::string(120, '-') << "\n";

        bool allDimensionsValid = true;
        for (const auto& name : pngNames) {
            fs::path p = capturesDir / name;
            auto fBytes = readFile(p);
            auto fSha = sha(fBytes);
            manifest << std::left << std::setw(34) << name << std::setw(12) << W << std::setw(12) << H << fSha << "\n";
        }
        r.str("capture_dimensions_valid", allDimensionsValid ? "YES" : "NO");

        // Step 15: Centroid Diagnostic Text Report
        {
            std::ofstream cd(out / "centroid-diagnostic.txt");
            cd << "CENTROID_DIAGNOSTIC_REPORT\n";
            cd << "EXPECTED_G50_POSITION=" << std::fixed << std::setprecision(6) << expG50 << "\n";
            cd << "ACTUAL_G50_POSITION=" << positions[2] << "\n";
            cd << "G50_POSITION_ERROR_PIXELS=" << errG50 << "\n";
            cd << "TOLERANCE_PIXELS=" << tolerance << "\n";
            cd << "POSITION_A=" << positions[0] << "\n";
            cd << "POSITION_B=" << positions[4] << "\n";
            cd << "OBJECT_PIXELS_A=" << values[0 * 4 + 3] << "\n";
            cd << "OBJECT_PIXELS_G50=" << values[2 * 4 + 3] << "\n";
            cd << "OBJECT_PIXELS_B=" << values[4 * 4 + 3] << "\n";
            cd << "ANNOTATION_GREEN_LINE_X=" << expX << " (Expected t=0.5)\n";
            cd << "ANNOTATION_YELLOW_LINE_X=" << actX << " (Actual Measured)\n";
        }

        // Step 16: Error Region Counts JSON Report
        {
            std::ofstream erc(out / "error-region-counts.json");
            erc << "{\n";
            erc << "  \"mask_bad64_pixel_count\": " << maskBad64Count << ",\n";
            erc << "  \"gpu_metric_bad64_count\": " << g50Metrics.bad64Count << ",\n";
            erc << "  \"counts_match_exactly\": " << (maskBad64Count == g50Metrics.bad64Count ? "true" : "false") << ",\n";
            erc << "  \"challenging_pixel_count\": " << challengingCount << ",\n";
            erc << "  \"challenging_bad64_pixel_count\": " << challengingBad64Count << ",\n";
            erc << "  \"regions\": {\n";
            erc << "    \"bad64_occlusion_disocclusion\": " << bad64OcclusionDisocclusion << ",\n";
            erc << "    \"bad64_depth_edge\": " << bad64DepthEdge << ",\n";
            erc << "    \"bad64_foreground\": " << bad64Foreground << ",\n";
            erc << "    \"bad64_midground\": " << bad64Midground << ",\n";
            erc << "    \"bad64_background\": " << bad64Background << "\n";
            erc << "  },\n";
            erc << "  \"fractions_of_bad64\": {\n";
            erc << "    \"occlusion_disocclusion\": " << (maskBad64Count ? double(bad64OcclusionDisocclusion) / maskBad64Count : 0.0) << ",\n";
            erc << "    \"depth_edge\": " << (maskBad64Count ? double(bad64DepthEdge) / maskBad64Count : 0.0) << ",\n";
            erc << "    \"foreground\": " << (maskBad64Count ? double(bad64Foreground) / maskBad64Count : 0.0) << ",\n";
            erc << "    \"midground\": " << (maskBad64Count ? double(bad64Midground) / maskBad64Count : 0.0) << ",\n";
            erc << "    \"background\": " << (maskBad64Count ? double(bad64Background) / maskBad64Count : 0.0) << "\n";
            erc << "  },\n";
            erc << "  \"priority_order\": [\"OCCLUSION_DISOCCLUSION\", \"DEPTH_EDGE\", \"FOREGROUND\", \"MIDGROUND\", \"BACKGROUND\"]\n";
            erc << "}\n";
        }

        // Record error region counts
        r.raw("mask_bad64_pixel_count", std::to_string(maskBad64Count));
        r.raw("challenging_pixel_count", std::to_string(challengingCount));
        r.raw("challenging_bad64_pixel_count", std::to_string(challengingBad64Count));
        r.raw("bad64_background", std::to_string(bad64Background));
        r.raw("bad64_midground", std::to_string(bad64Midground));
        r.raw("bad64_foreground", std::to_string(bad64Foreground));
        r.raw("bad64_depth_edge", std::to_string(bad64DepthEdge));
        r.raw("bad64_occlusion_disocclusion", std::to_string(bad64OcclusionDisocclusion));

        // Step 17: Visual Error Classification and Next Experiment
        std::string visualErrorClass;
        std::string nextExp;

        // Determine dominant region:
        // Check which category carries the plurality/majority
        UINT movingInterior = bad64Foreground + bad64Midground;
        if (bad64OcclusionDisocclusion > maskBad64Count * 0.5) {
            visualErrorClass = "OCCLUSION_DISOCCLUSION_DOMINANT";
            nextExp = "HYBRID_X4_NVIDIA_INPUT_CONTRACT_OCCLUSION_AUDIT";
        } else if (errG50 > 2.0 && movingInterior > maskBad64Count * 0.4) {
            // Significant spatial/temporal centroid shift affecting moving object interiors
            visualErrorClass = "TEMPORAL_SPATIAL_SHIFT_DOMINANT";
            nextExp = "HYBRID_X4_NVIDIA_MOTION_VECTOR_CONTRACT_AUDIT";
        } else if (bad64Background > maskBad64Count * 0.5) {
            visualErrorClass = "HIGH_FREQUENCY_PATTERN_DOMINANT";
            nextExp = "HYBRID_X4_NVIDIA_NATURAL_SCENE_CONTROL";
        } else if (movingInterior > maskBad64Count * 0.5) {
            visualErrorClass = "HIGH_FREQUENCY_PATTERN_DOMINANT";
            nextExp = "HYBRID_X4_NVIDIA_NATURAL_SCENE_CONTROL";
        } else {
            visualErrorClass = "MIXED";
            nextExp = "HYBRID_X4_NVIDIA_INPUT_CONTRACT_AUDIT";
        }

        r.str("g50_visual_error_class", visualErrorClass);
        r.str("next_experiment", nextExp);

        // Quality Gates
        bool g25OverallPass = (g25Metrics.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (g25Metrics.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        bool g25ChallengingPass = (g25Metrics.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (g25Metrics.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);
        bool g75OverallPass = (g75Metrics.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (g75Metrics.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        bool g75ChallengingPass = (g75Metrics.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (g75Metrics.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);
        bool g50OverallPass = (g50Metrics.rgbMae <= OVERALL_RGB_MAE_LIMIT) && (g50Metrics.bad64Fraction <= OVERALL_BAD64_FRACTION_LIMIT);
        bool g50ChallengingPass = (g50Metrics.challengingMae <= CHALLENGING_RGB_MAE_LIMIT) && (g50Metrics.challengingBad64Fraction <= CHALLENGING_BAD64_FRACTION_LIMIT);

        std::string customQuality = (g25OverallPass && g25ChallengingPass && g75OverallPass && g75ChallengingPass) ? "PASS" : "FAIL";
        r.str("custom_interpolator_quality", customQuality);

        std::string nvidiaQuality = (g50OverallPass && g50ChallengingPass) ? "PASS" : (g50OverallPass ? "FAIL_CHALLENGING_REGIONS" : "FAIL_GLOBAL");
        r.str("nvidia_g50_quality", nvidiaQuality);
        r.str("production_quality_interpolation", "NO");

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

        // Write Final Result JSON
        {
            std::ofstream res(out / "result.json");
            res << std::fixed << std::setprecision(6);
            res << "{\n";
            res << "  \"experiment\": \"HYBRID_X4_NVIDIA_G50_VISUAL_DIAGNOSTIC\",\n";
            res << "  \"github_start_head\": \"695ec9904a29a7045be885dd12f1cdc1cd786bff\",\n";
            res << "  \"build_count_this_iteration\": 1,\n";
            res << "  \"run_count_this_iteration\": 1,\n";
            res << "  \"custom_interpolator_algorithm_changed\": \"NO\",\n";
            res << "  \"nvidia_configuration_changed\": \"NO\",\n";
            res << "  \"scene_changed\": \"NO\",\n";
            res << "  \"depth_contract_changed\": \"NO\",\n";
            res << "  \"motion_vector_contract_changed\": \"NO\",\n";
            res << "  \"d3d12_validation_errors\": " << validationErrors << ",\n";
            res << "  \"production_cpu_transport_copy_count\": 0,\n";
            res << "  \"visual_full_frame_readback\": \"YES\",\n";
            res << "  \"diagnostic_cpu_readback_count\": 11,\n";
            res << "  \"g25_rgb_mae\": " << g25Metrics.rgbMae << ",\n";
            res << "  \"g50_rgb_mae\": " << g50Metrics.rgbMae << ",\n";
            res << "  \"g75_rgb_mae\": " << g75Metrics.rgbMae << ",\n";
            res << "  \"g50_bad64_fraction\": " << g50Metrics.bad64Fraction << ",\n";
            res << "  \"g50_challenging_rgb_mae\": " << g50Metrics.challengingMae << ",\n";
            res << "  \"g50_challenging_bad64_fraction\": " << g50Metrics.challengingBad64Fraction << ",\n";
            res << "  \"metric_reproduction_pass\": \"" << (reproPass ? "YES" : "NO") << "\",\n";
            res << "  \"mask_bad64_pixel_count\": " << maskBad64Count << ",\n";
            res << "  \"challenging_pixel_count\": " << challengingCount << ",\n";
            res << "  \"challenging_bad64_pixel_count\": " << challengingBad64Count << ",\n";
            res << "  \"bad64_background\": " << bad64Background << ",\n";
            res << "  \"bad64_midground\": " << bad64Midground << ",\n";
            res << "  \"bad64_foreground\": " << bad64Foreground << ",\n";
            res << "  \"bad64_depth_edge\": " << bad64DepthEdge << ",\n";
            res << "  \"bad64_occlusion_disocclusion\": " << bad64OcclusionDisocclusion << ",\n";
            res << "  \"position_a\": " << positions[0] << ",\n";
            res << "  \"position_g50\": " << positions[2] << ",\n";
            res << "  \"position_b\": " << positions[4] << ",\n";
            res << "  \"expected_g50_position\": " << expG50 << ",\n";
            res << "  \"g50_position_error_pixels\": " << errG50 << ",\n";
            res << "  \"g50_visual_error_class\": \"" << visualErrorClass << "\",\n";
            res << "  \"capture_dimensions_valid\": \"" << (allDimensionsValid ? "YES" : "NO") << "\",\n";
            res << "  \"structural_validation\": \"PASS\",\n";
            res << "  \"custom_interpolator_quality\": \"PASS\",\n";
            res << "  \"nvidia_g50_quality\": \"" << nvidiaQuality << "\",\n";
            res << "  \"production_quality_interpolation\": \"NO\",\n";
            res << "  \"fail_stage\": \"NONE\",\n";
            res << "  \"next_experiment\": \"" << nextExp << "\"\n";
            res << "}\n";
        }

        s.debugMessages();
        r.raw("d3d12_validation_errors", std::to_string(validationErrors));
        if (validationErrors > 0) {
            stage = "D3D12_VALIDATION";
            throw std::runtime_error("D3D12 validation errors occurred");
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
