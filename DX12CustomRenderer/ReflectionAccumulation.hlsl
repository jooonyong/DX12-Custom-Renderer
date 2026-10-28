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
    uint2 Pixel = DispatchThreadID.xy;
    if(Pixel.x >= Width || Pixel.y >= Height)
    {
        return;
    }
    
    float3 CurrentColor = float3(0.0f, 0.0f, 0.0f);
    uint TotalWeight = 0;
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1;j++)
        {
            int2 Offset = int2(i, j);
            if ((Pixel + Offset).x < 0 || (Pixel + Offset).y < 0 || (Pixel + Offset).x >= Width || (Pixel + Offset).y >= Height)
            {
                continue;
            }
            
            uint GausianWeight;
            if (abs(i) + abs(j) == 2)
            {
                GausianWeight = 1;
                TotalWeight += GausianWeight;
            }
            else if(abs(i)+abs(j) == 1)
            {
                GausianWeight = 2;
                TotalWeight += GausianWeight;
            }
            else if(abs(i)+abs(j) == 0)
            {
                GausianWeight = 4;
                TotalWeight += GausianWeight;
            }
            CurrentColor += (CurrentReflection.Load(int3(Pixel + Offset, 0)).xyz) * GausianWeight;
        }
    }
    CurrentColor = CurrentColor / max(TotalWeight, 1.0f);
    if (any(isnan(CurrentColor)) || any(isinf(CurrentColor)))
    {
        OutHistory[Pixel] = float4(1.0f, 0.0f, 1.0f, 1.0f);
        return;
    }
    if (any(isnan(PrevHistory.Load(int3(Pixel, 0)).xyz)) || any(isinf(PrevHistory.Load(int3(Pixel, 0)).xyz)))
    {
        OutHistory[Pixel] = float4(0.0f, 1.0f, 0.0f, 1.0f);
        return;
    }
    if(HistoryFrameCount == 0)
    {
        OutHistory[Pixel] = float4(CurrentReflection.Load(int3(Pixel, 0)).xyz, 1.0f);
        return;
    }
    float3 PrevColor = PrevHistory.Load(int3(Pixel, 0)).xyz;
    float Weight = 1.0f / (HistoryFrameCount + 1.0f);
    float3 AccumulatedColor = lerp(PrevColor, CurrentColor, Weight);
    
    OutHistory[Pixel] = float4(AccumulatedColor, 1.0f);
}