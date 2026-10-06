// HLSL Shaders for Hybrid Multiplier X3 Prototype
// Exact baseline HybridInterpolationCS algorithm preserved from hybrid-mfg-x4 confirmed baseline
// Scene Fixture generates:
//   - Projected device depth (near=0.1, far=100.0)
//   - CustomMotion: NDC clip space (for custom interpolator)
//   - NvidiaMotion: Screen-space pixels (for official NVIDIA DLSS FG contract)

Texture2D<float4> ColorA : register(t0);
Texture2D<float4> ColorB : register(t1);
Texture2D<float2> MotionA : register(t2);
Texture2D<float2> MotionB : register(t3);
Texture2D<float> DepthA : register(t4);
Texture2D<float> DepthB : register(t5);
Texture2D<float> RefDepthTex : register(t6);

RWTexture2D<float4> Output : register(u0);
RWTexture2D<float> FixtureDepth : register(u1);
RWTexture2D<float2> CustomMotion : register(u2);
RWTexture2D<float2> NvidiaMotion : register(u3);
RWByteAddressBuffer Stats : register(u4);

SamplerState LinearClamp : register(s0);
cbuffer Parameters : register(b0) { uint Width; uint Height; uint Frame; float Time; uint StatOffset; }

// Continuous deterministic multi-layer scene with projected perspective device depth.
[numthreads(8,8,1)]
void FixtureCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    float T = Time;
    float2 pos = float2(id.xy);

    // 1. Background layer: camera-like panning motion (world displacement +8*T, +4*T)
    // viewZ = 5.0 -> deviceDepth = 100/99.9 - 10/(99.9 * 5.0) = 98.0 / 99.9 (~0.980981)
    float camX = 8.0f * T;
    float camY = 4.0f * T;
    float2 bgCoord = pos - float2(camX, camY) + 10240.0f;
    uint bgPat = (uint(bgCoord.x / 16.0f) + uint(bgCoord.y / 16.0f)) & 1u;
    float3 color = bgPat ? float3(0.08f, 0.18f, 0.32f) : float3(0.16f, 0.28f, 0.44f);
    float depth = 100.0f / 99.9f - 10.0f / (99.9f * 5.0f); // viewZ = 5.0
    float2 motionPixels = float2(-8.0f, -4.0f); // displacement to previous frame (T - 1.0)

    // 2. Midground layer: medium object traversing horizontally and vertically
    // viewZ = 1.0 -> deviceDepth = 100/99.9 - 10/(99.9 * 1.0) = 90.0 / 99.9 (~0.900901)
    float midX = 420.0f + 16.0f * T;
    float midY = 220.0f + 12.0f * T;
    if (pos.x >= midX && pos.x < midX + 360.0f && pos.y >= midY && pos.y < midY + 260.0f) {
        uint midPat = uint((pos.x - midX + pos.y - midY) / 14.0f) & 1u;
        color = midPat ? float3(0.85f, 0.55f, 0.15f) : float3(0.48f, 0.28f, 0.08f);
        depth = 100.0f / 99.9f - 10.0f / (99.9f * 1.0f); // viewZ = 1.0
        motionPixels = float2(-16.0f, -12.0f);
    }

    // 3. Foreground layer: faster object moving across and occluding midground/background
    // viewZ = 0.5 -> deviceDepth = 100/99.9 - 10/(99.9 * 0.5) = 80.0 / 99.9 (~0.800801)
    float fgX = 320.0f + 28.0f * T;
    float fgY = 290.0f - 8.0f * T;
    if (pos.x >= fgX && pos.x < fgX + 240.0f && pos.y >= fgY && pos.y < fgY + 240.0f) {
        uint fgPat = (uint((pos.x - fgX) / 12.0f) ^ uint((pos.y - fgY) / 12.0f)) & 1u;
        color = fgPat ? float3(0.95f, 0.20f, 0.25f) : float3(0.65f, 0.10f, 0.50f);
        depth = 100.0f / 99.9f - 10.0f / (99.9f * 0.5f); // viewZ = 0.5
        motionPixels = float2(-28.0f, 8.0f);
    }

    // Convert motion in pixels to NDC clip space (current_to_previous) for Custom Interpolator
    float2 mvecNDC = float2(2.0f * motionPixels.x / float(Width), -2.0f * motionPixels.y / float(Height));

    Output[id.xy] = float4(color, 1.0f);
    FixtureDepth[id.xy] = depth;
    CustomMotion[id.xy] = mvecNDC;
    NvidiaMotion[id.xy] = motionPixels;
}

bool inside(float2 pixel) {
    return all(pixel >= 0.0f) && all(pixel <= float2(Width - 1u, Height - 1u));
}
float2 uv(float2 pixel) { return (pixel + 0.5f) / float2(Width, Height); }

// BASELINE ALGORITHM: functionally identical to baseline experiments
[numthreads(8,8,1)]
void HybridInterpolationCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    float2 pixel = id.xy;
    float za = DepthA.Load(int3(id.xy, 0));
    float zb = DepthB.Load(int3(id.xy, 0));
    float2 backwardClip = za < zb ? MotionA.Load(int3(id.xy, 0)) : MotionB.Load(int3(id.xy, 0));
    float2 backwardPixels = backwardClip * float2(0.5f, -0.5f) * float2(Width, Height);
    float2 toA = pixel + Time * backwardPixels;
    float2 toB = pixel - (1.0f - Time) * backwardPixels;
    bool validA = inside(toA), validB = inside(toB);
    float4 a = ColorA.SampleLevel(LinearClamp, uv(toA), 0.0f);
    float4 b = ColorB.SampleLevel(LinearClamp, uv(toB), 0.0f);
    float depthA = DepthA.SampleLevel(LinearClamp, uv(toA), 0.0f);
    float depthB = DepthB.SampleLevel(LinearClamp, uv(toB), 0.0f);
    float4 value;
    if (!validA && !validB) value = Time < 0.5f ? ColorA.Load(int3(id.xy, 0)) : ColorB.Load(int3(id.xy, 0));
    else if (!validA) value = b;
    else if (!validB) value = a;
    else if (abs(depthA - depthB) > 0.005f) value = depthA < depthB ? a : b;
    else value = lerp(a, b, Time);
    Output[id.xy] = value;
}

// Bounded reduction kernel for structural validation:
// 64-bit coordinate-dependent hash + foreground centroid accumulator
[numthreads(8,8,1)]
void ReductionCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    uint4 rgba = (uint4)round(saturate(ColorA.Load(int3(id.xy, 0))) * 255.0f);
    uint index = id.y * Width + id.x;
    uint h = 2166136261u;
    h = (h ^ rgba.r) * 16777619u; h = (h ^ rgba.g) * 16777619u;
    h = (h ^ rgba.b) * 16777619u; h = (h ^ rgba.a) * 16777619u;
    h = (h ^ index) * 16777619u;
    Stats.InterlockedXor(StatOffset, h);
    Stats.InterlockedAdd(StatOffset + 4u, h * 2246822519u + index);
    if (rgba.r > 160u && rgba.g < 60u) {
        Stats.InterlockedAdd(StatOffset + 8u, id.x);
        Stats.InterlockedAdd(StatOffset + 12u, 1u);
    }
}

// GPU Quality metrics against Ground Truth Reference
// Measures RGB_MAE, BAD32, BAD64, and challenging region breakdown
[numthreads(8,8,1)]
void QualityMetricsCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    float3 cand = saturate(ColorA.Load(int3(id.xy, 0)).rgb);
    float3 refCol = saturate(ColorB.Load(int3(id.xy, 0)).rgb);
    float3 diff = abs(cand - refCol);
    float mae = (diff.r + diff.g + diff.b) / 3.0f;
    float maxDiff255 = max(diff.r, max(diff.g, diff.b)) * 255.0f;
    uint bad32 = maxDiff255 > 32.0f ? 1u : 0u;
    uint bad64 = maxDiff255 > 64.0f ? 1u : 0u;

    // Detect challenging regions: depth discontinuity or occlusion/disocclusion
    float refZ = RefDepthTex.Load(int3(id.xy, 0));
    float zA   = DepthA.Load(int3(id.xy, 0));
    float zB   = DepthB.Load(int3(id.xy, 0));
    
    // Check neighbor depth variation in reference frame (depth edge)
    float zUp   = RefDepthTex.Load(int3(clamp(int(id.x), 0, int(Width)-1), clamp(int(id.y)-1, 0, int(Height)-1), 0));
    float zDown = RefDepthTex.Load(int3(clamp(int(id.x), 0, int(Width)-1), clamp(int(id.y)+1, 0, int(Height)-1), 0));
    float zLeft = RefDepthTex.Load(int3(clamp(int(id.x)-1, 0, int(Width)-1), clamp(int(id.y), 0, int(Height)-1), 0));
    float zRight= RefDepthTex.Load(int3(clamp(int(id.x)+1, 0, int(Width)-1), clamp(int(id.y), 0, int(Height)-1), 0));
    float maxEdgeZ = max(max(abs(refZ - zUp), abs(refZ - zDown)), max(abs(refZ - zLeft), abs(refZ - zRight)));
    
    bool isDepthEdge = (maxEdgeZ > 0.05f);
    bool isOcclOrDisoccl = (abs(refZ - zA) > 0.05f) || (abs(refZ - zB) > 0.05f);
    bool isChallenging = isDepthEdge || isOcclOrDisoccl;

    uint offset = StatOffset;
    uint maeUnits = (uint)round(mae * 4000.0f);
    
    Stats.InterlockedAdd(offset + 0u, 1u);
    Stats.InterlockedAdd(offset + 4u, maeUnits);
    Stats.InterlockedAdd(offset + 8u, bad32);
    Stats.InterlockedAdd(offset + 12u, bad64);
    if (isChallenging) {
        Stats.InterlockedAdd(offset + 16u, 1u);
        Stats.InterlockedAdd(offset + 20u, maeUnits);
        Stats.InterlockedAdd(offset + 24u, bad32);
        Stats.InterlockedAdd(offset + 28u, bad64);
    }
}
