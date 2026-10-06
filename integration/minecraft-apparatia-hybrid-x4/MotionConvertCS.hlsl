Texture2D<float2> InMotion : register(t0);
RWTexture2D<float2> OutNDC : register(u0);
cbuffer Params : register(b0) { uint Width; uint Height; }

[numthreads(8,8,1)]
void MotionConvertCS(uint3 id : SV_DispatchThreadID) {
    if (id.x >= Width || id.y >= Height) return;
    float2 m = InMotion.Load(int3(id.xy, 0));
    OutNDC[id.xy] = float2(2.0f * m.x, -2.0f * m.y);
}
