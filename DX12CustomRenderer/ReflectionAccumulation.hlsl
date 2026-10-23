Texture2D<float4> CurrentReflection : register(t0);
Texture2D<float4> PrevHistory : register(t1);
RWTexture2D<float4> OutHistory : register(u0);

cbuffer AccumulationConstants : register(b0)
{
    uint Width;
    uint Height;
    uint HistoryFrameCount;
    uint Padding;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // TODO
    uint2 Pixel = DispatchThreadID.xy;
    if(Pixel.x >= Width || Pixel.y >= Height)
    {
        return;
    }
    
    float3 CurrentColor = CurrentReflection.Load(int3(Pixel, 0)).xyz;
    if(HistoryFrameCount == 0)
    {
        OutHistory[Pixel] = float4(CurrentColor, 1.0f);
        return;
    }
    float3 PrevColor = PrevHistory.Load(int3(Pixel, 0)).xyz;
    float Weight = 1.0f / (HistoryFrameCount + 1.0f);
    float3 AccumulatedColor = lerp(PrevColor, CurrentColor, Weight);
    
    OutHistory[Pixel] = float4(AccumulatedColor, 1.0f);

}