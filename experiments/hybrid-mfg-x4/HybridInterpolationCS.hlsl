// Prototype only. G50 is never bound as input to HybridInterpolationCS.
Texture2D<float4> ColorA : register(t0);
Texture2D<float4> ColorB : register(t1);
Texture2D<float2> MotionA : register(t2);
Texture2D<float2> MotionB : register(t3);
Texture2D<float> DepthA : register(t4);
Texture2D<float> DepthB : register(t5);
RWTexture2D<float4> Output : register(u0);
RWTexture2D<float> FixtureDepth : register(u1);
RWTexture2D<float2> FixtureMotion : register(u2);
RWByteAddressBuffer Stats : register(u3);
SamplerState LinearClamp : register(s0);
cbuffer Parameters : register(b0) { uint Width; uint Height; uint Frame; float Time; uint StatOffset; }

[numthreads(8,8,1)]
void FixtureCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    uint left = 300u + 16u * Frame;
    bool object = id.x >= left && id.x < left + 192u && id.y >= 248u && id.y < 472u;
    uint pattern = (((object ? (id.x - left) : id.x) / 12u + id.y / 12u) & 1u);
    float3 color = object ? float3(190u + 40u * pattern, 70u + 30u * pattern, 25u + 20u * pattern)
                          : float3(20u + 12u * pattern, 65u + 12u * pattern, 115u + 12u * pattern);
    Output[id.xy] = float4(color / 255.0f, 1.0f);
    FixtureDepth[id.xy] = 100.0f / 99.9f - 10.0f / (99.9f * (object ? 2.0f : 10.0f));
    FixtureMotion[id.xy] = object ? float2(-2.0f * 16.0f / float(Width), 0.0f) : float2(0.0f, 0.0f);
}

bool inside(float2 pixel) {
    return all(pixel >= 0.0f) && all(pixel <= float2(Width - 1u, Height - 1u));
}
float2 uv(float2 pixel) { return (pixel + 0.5f) / float2(Width, Height); }

[numthreads(8,8,1)]
void HybridInterpolationCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    float2 pixel = id.xy;
    // The current-to-previous vector is negative X for this positive-X fixture.
    // Approximate the inverse flow with A's corresponding surface vector.
    float za = DepthA.Load(int3(id.xy,0));
    float zb = DepthB.Load(int3(id.xy,0));
    float2 backwardClip = za < zb ? MotionA.Load(int3(id.xy,0)) : MotionB.Load(int3(id.xy,0));
    float2 backwardPixels = backwardClip * float2(0.5f, -0.5f) * float2(Width, Height);
    float2 toA = pixel + Time * backwardPixels;
    float2 toB = pixel - (1.0f - Time) * backwardPixels;
    bool validA = inside(toA), validB = inside(toB);
    float4 a = ColorA.SampleLevel(LinearClamp, uv(toA), 0.0f);
    float4 b = ColorB.SampleLevel(LinearClamp, uv(toB), 0.0f);
    float depthA = DepthA.SampleLevel(LinearClamp, uv(toA), 0.0f);
    float depthB = DepthB.SampleLevel(LinearClamp, uv(toB), 0.0f);
    float4 value;
    if (!validA && !validB) value = Time < 0.5f ? ColorA.Load(int3(id.xy,0)) : ColorB.Load(int3(id.xy,0));
    else if (!validA) value = b;
    else if (!validB) value = a;
    else if (abs(depthA - depthB) > 0.005f) value = depthA < depthB ? a : b;
    else value = lerp(a, b, Time);
    Output[id.xy] = value;
}

// Bounded two-word fingerprint and foreground centroid, not a full-image readback.
[numthreads(8,8,1)]
void ReductionCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    uint4 rgba = (uint4)round(saturate(ColorA.Load(int3(id.xy,0))) * 255.0f);
    uint index = id.y * Width + id.x;
    uint h = 2166136261u;
    h = (h ^ rgba.r) * 16777619u; h = (h ^ rgba.g) * 16777619u;
    h = (h ^ rgba.b) * 16777619u; h = (h ^ rgba.a) * 16777619u;
    h = (h ^ index) * 16777619u;
    Stats.InterlockedXor(StatOffset, h);
    Stats.InterlockedAdd(StatOffset + 4u, h * 2246822519u + index);
    if (rgba.r > 150u && rgba.g < 140u && rgba.b < 90u) {
        Stats.InterlockedAdd(StatOffset + 8u, id.x);
        Stats.InterlockedAdd(StatOffset + 12u, 1u);
    }
}
