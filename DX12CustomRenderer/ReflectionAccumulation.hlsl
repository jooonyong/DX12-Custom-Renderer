Texture2D<float4> CurrentReflection : register(t0);
Texture2D<float4> PrevHistory : register(t1);
RWTexture2D<float4> OutHistory : register(u0);
Texture2D<float4> GBufferB : register(t2); //normal
Texture2D<float> SceneDepth : register(t3); //Depth

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
    float3 Normal = normalize(GBufferB.Load(int3(Pixel, 0)).xyz);
    float NormalWeight = 0;
    float TotalWeight = 0;
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
            }
            else if(abs(i)+abs(j) == 1)
            {
                GausianWeight = 2;
            }
            else if(abs(i)+abs(j) == 0)
            {
                GausianWeight = 4;
            }
     
            float3 NeighborNormal = normalize(GBufferB.Load(int3(Pixel + Offset, 0)).xyz);
            NormalWeight = pow(saturate(dot(Normal, NeighborNormal)), 16);
            
            float CombinedWeight = float(GausianWeight) * NormalWeight;
            
            CurrentColor += (CurrentReflection.Load(int3(Pixel + Offset, 0)).xyz) * CombinedWeight;
            TotalWeight += CombinedWeight;
        }
    }
    CurrentColor = CurrentColor / max(TotalWeight, 0.0001f);
    if (any(isnan(CurrentColor)) || any(isinf(CurrentColor)))
    {
        OutHistory[Pixel] = float4(1.0f, 0.0f, 1.0f, 1.0f);
        return;
    }
    
    if(HistoryFrameCount == 0)
    {
        OutHistory[Pixel] = float4(CurrentColor, 1.0f);
        return;
    }
    float3 PrevColor = PrevHistory.Load(int3(Pixel, 0)).xyz;
    if (any(isnan(PrevHistory.Load(int3(Pixel, 0)).xyz)) || any(isinf(PrevHistory.Load(int3(Pixel, 0)).xyz)))
    {
        OutHistory[Pixel] = float4(0.0f, 1.0f, 0.0f, 1.0f);
        return;
    }
    
    float Weight = 1.0f / (HistoryFrameCount + 1.0f);
    float3 AccumulatedColor = lerp(PrevColor, CurrentColor, Weight);
    
    OutHistory[Pixel] = float4(AccumulatedColor, 1.0f);
}