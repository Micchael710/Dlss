// HLSL Shaders for Corrected Motion Vector Confirmation
// Restores 100% exact baseline HybridInterpolationCS algorithm from hybrid-mfg-x4-depth-contract
// Scene Fixture generates:
//   - Projected device depth (near=0.1, far=100.0)
//   - CustomMotion: NDC clip space (for historical custom interpolator)
//   - NvidiaMotion: Screen-space pixels (for official NVIDIA DLSS FG contract)

Texture2D<float4> ColorA : register(t0);
Texture2D<float4> ColorB : register(t1);
Texture2D<float2> MotionA : register(t2);
Texture2D<float2> MotionB : register(t3);
Texture2D<float> DepthA : register(t4);
Texture2D<float> DepthB : register(t5);
Texture2D<float4> HudlessTex : register(t6);

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

float3 RGBToYCoCg(float3 rgb) {
    float Y  = dot(rgb, float3( 0.25f, 0.5f,  0.25f));
    float Co = dot(rgb, float3( 0.5f,  0.0f, -0.5f));
    float Cg = dot(rgb, float3(-0.25f, 0.5f, -0.25f));
    return float3(Y, Co, Cg);
}

float3 YCoCgToRGB(float3 ycocg) {
    float Y  = ycocg.x;
    float Co = ycocg.y;
    float Cg = ycocg.z;
    return float3(
        Y + Co - Cg,
        Y + Cg,
        Y - Co - Cg
    );
}

// PORTED FROM AUTHORITATIVE FRIEND SOURCE (FSRFG_wisteria-main / fsr_fg_compute.comp)
[numthreads(8,8,1)]
void HybridInterpolationCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    int2 pixelCoord = int2(id.xy);

    float2 texelSize = 1.0f / float2(Width, Height);
    float2 uvCoord = (float2(pixelCoord) + 0.5f) * texelSize;

    float4 currColor = ColorB.SampleLevel(LinearClamp, uvCoord, 0.0f);
    float4 prevColor = ColorA.SampleLevel(LinearClamp, uvCoord, 0.0f);

    // If history is reset (teleport, window resize, camera cut), smoothly output current frame
    if (Frame == 0u) {
        Output[pixelCoord] = currColor;
        return;
    }

    // HUD isolation: detect 2D UI elements (chat, crosshair, hotbar, menus)
    float4 hudlessColor = HudlessTex.SampleLevel(LinearClamp, uvCoord, 0.0f);
    float3 hudDiff = abs(currColor.rgb - hudlessColor.rgb);
    float maxHudDiff = max(hudDiff.r, max(hudDiff.g, hudDiff.b));
    if (maxHudDiff > 0.02f) {
        Output[pixelCoord] = currColor;
        return;
    }

    // Depth & Hand isolation
    float depth = DepthB.SampleLevel(LinearClamp, uvCoord, 0.0f).r;
    float2 motionNDC = MotionB.SampleLevel(LinearClamp, uvCoord, 0.0f).xy;
    if (!(abs(motionNDC.x) < 1000.0f) || !(abs(motionNDC.y) < 1000.0f)) {
        motionNDC = float2(0.0f, 0.0f);
    }

    // MotionConvertCS mapped: OutNDC = float2(2.0f * mv.x, -2.0f * mv.y)
    // Convert NDC motion back to UV space motion vector:
    float2 mv = float2(motionNDC.x * 0.5f, -motionNDC.y * 0.5f);

    // First-person hand and held items in Minecraft/Iris:
    // Hand depth <= 0.56 in normalized screen depth
    bool isHand = (depth > 0.0f && depth <= 0.56f);
    if (isHand) {
        mv = float2(0.0f, 0.0f);
    }

    // Reprojection coordinates
    float lambda = Time; // 0.25f for G25, 0.75f for G75
    float2 uvPrev = clamp(uvCoord + lambda * mv, 0.0f, 1.0f);
    float2 uvCurr = clamp(uvCoord - (1.0f - lambda) * mv, 0.0f, 1.0f);
    float4 colPrev = ColorA.SampleLevel(LinearClamp, uvPrev, 0.0f);
    float4 colCurr = ColorB.SampleLevel(LinearClamp, uvCurr, 0.0f);

    float3 initDiffV = abs(colPrev.rgb - colCurr.rgb);
    float initialDiff = max(initDiffV.r, max(initDiffV.g, initDiffV.b));

    // Software Optical Flow (Local Block Matching) for Sable Sublevels & Physics Contraptions
    if (!isHand && (dot(mv, mv) < 1e-7f || initialDiff > 0.12f)) {
        float2 bestOffset = lambda * mv;
        float bestDiff = initialDiff;

        const int R = 6;
        for (int dy = -R; dy <= R; dy += 2) {
            for (int dx = -R; dx <= R; dx += 2) {
                if (dx == 0 && dy == 0) continue;
                float2 offset = float2(float(dx), float(dy)) * texelSize;
                float2 testUvPrev = clamp(uvCoord + offset, 0.0f, 1.0f);
                float4 testColPrev = ColorA.SampleLevel(LinearClamp, testUvPrev, 0.0f);
                float3 d = abs(testColPrev.rgb - colCurr.rgb);
                float diff = max(d.r, max(d.g, d.b));
                if (diff < bestDiff) {
                    bestDiff = diff;
                    bestOffset = offset;
                }
            }
        }

        // If local optical flow found a superior match in prevColor:
        if (bestDiff < initialDiff - 0.05f) {
            uvPrev = clamp(uvCoord + bestOffset, 0.0f, 1.0f);
            float factor = (1.0f - lambda) / max(lambda, 0.01f);
            uvCurr = clamp(uvCoord - bestOffset * factor, 0.0f, 1.0f);
            colPrev = ColorA.SampleLevel(LinearClamp, uvPrev, 0.0f);
            colCurr = ColorB.SampleLevel(LinearClamp, uvCurr, 0.0f);
        }
    }

    // YCoCg 3x3 Neighborhood Clamping for Water Flickering & Specular Stability
    float3 ycocgMin = float3(1e9f, 1e9f, 1e9f);
    float3 ycocgMax = float3(-1e9f, -1e9f, -1e9f);

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float2 sampleUv = clamp(uvCurr + float2(float(x), float(y)) * texelSize, 0.0f, 1.0f);
            float3 sampleYCoCg = RGBToYCoCg(ColorB.SampleLevel(LinearClamp, sampleUv, 0.0f).rgb);
            ycocgMin = min(ycocgMin, sampleYCoCg);
            ycocgMax = max(ycocgMax, sampleYCoCg);
        }
    }

    float3 boxMargin = (ycocgMax - ycocgMin) * 0.05f;
    ycocgMin -= boxMargin;
    ycocgMax += boxMargin;

    // Clamp previous sample into the temporal bounding box of current frame
    float3 prevYCoCg = RGBToYCoCg(colPrev.rgb);
    float3 clampedPrevYCoCg = clamp(prevYCoCg, ycocgMin, ycocgMax);
    float3 clampedColPrev = YCoCgToRGB(clampedPrevYCoCg);

    // Compute temporal motion interpolation
    float3 motionColor = lerp(clampedColPrev, colCurr.rgb, lambda);

    // Smooth disocclusion fallback: if color difference is still large, blend towards colCurr
    float3 diff = abs(clampedColPrev - colCurr.rgb);
    float maxDiff = max(diff.r, max(diff.g, diff.b));
    float disocclusion = saturate((maxDiff - 0.25f) / 0.25f);
    float3 finalRgb = lerp(motionColor, colCurr.rgb, disocclusion);

    Output[pixelCoord] = float4(finalRgb, colCurr.a);
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
    float refZ = HudlessTex.Load(int3(id.xy, 0)).r;
    float zA   = DepthA.Load(int3(id.xy, 0));
    float zB   = DepthB.Load(int3(id.xy, 0));
    
    // Check neighbor depth variation in reference frame (depth edge)
    float zUp   = HudlessTex.Load(int3(clamp(int(id.x), 0, int(Width)-1), clamp(int(id.y)-1, 0, int(Height)-1), 0)).r;
    float zDown = HudlessTex.Load(int3(clamp(int(id.x), 0, int(Width)-1), clamp(int(id.y)+1, 0, int(Height)-1), 0)).r;
    float zLeft = HudlessTex.Load(int3(clamp(int(id.x)-1, 0, int(Width)-1), clamp(int(id.y), 0, int(Height)-1), 0)).r;
    float zRight= HudlessTex.Load(int3(clamp(int(id.x)+1, 0, int(Width)-1), clamp(int(id.y), 0, int(Height)-1), 0)).r;
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
