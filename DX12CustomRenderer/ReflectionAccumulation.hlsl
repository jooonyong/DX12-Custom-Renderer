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

cbuffer ReconstructionConstants : register(b1)
{
    float4x4 InverseViewProjection;
}

float3 ReconstructWorldPosition(int2 Pixel, float Depth)
{
    float2 UV = (float2(Pixel) + 0.5f) / float2(Width, Height);
    float2 NDC = UV * 2.0f - 1.0f;
    NDC.y = -NDC.y;
    
    float4 Position = float4(NDC.x, NDC.y, Depth, 1.0f);
    float4 WorldPosition = mul(Position, InverseViewProjection);
    WorldPosition.xyz /= WorldPosition.w;
    
    return WorldPosition.xyz;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    int2 Pixel = DispatchThreadID.xy;
    if(Pixel.x >= Width || Pixel.y >= Height)
    {
        return;
    }
    
    float3 CurrentColor = float3(0.0f, 0.0f, 0.0f);
    float3 Normal = GBufferB.Load(int3(Pixel, 0)).xyz;
    float NormalLengthSq = dot(Normal, Normal);
    if (!isfinite(NormalLengthSq) || NormalLengthSq <= 1e-8f)
    {
        OutHistory[Pixel] = float4(0, 0, 0, 1);
        return;
    }

    Normal = Normal * rsqrt(NormalLengthSq);
    float NormalWeight = 0;
    float TotalWeight = 0;
    
    float Depth = SceneDepth.Load(int3(Pixel, 0));
    if (Depth >= 1.0f)
    {
        OutHistory[Pixel] = float4(0, 0, 0, 1);
        return;
    }
    
    float3 WorldPosition = ReconstructWorldPosition(Pixel, Depth);
    
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
            float NeighborDepth = SceneDepth.Load(int3(Pixel + Offset, 0));
            float3 NeighborPosition = ReconstructWorldPosition(Pixel + Offset, NeighborDepth);
            float Distance = abs(dot(NeighborPosition - WorldPosition, Normal));
            float DepthWeight = exp(-Distance / 0.05f);
            
            float3 NeighborNormal = normalize(GBufferB.Load(int3(Pixel + Offset, 0)).xyz);
            NormalWeight = pow(saturate(dot(Normal, NeighborNormal)), 16);
            
            float CombinedWeight = float(GausianWeight) * NormalWeight * DepthWeight;
            
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
    if (any(isnan(PrevColor)) || any(isinf(PrevColor)))
    {
        OutHistory[Pixel] = float4(0.0f, 1.0f, 0.0f, 1.0f);
        return;
    }
    
    float Weight = 1.0f / (HistoryFrameCount + 1.0f);
    float3 AccumulatedColor = lerp(PrevColor, CurrentColor, Weight);
    
    OutHistory[Pixel] = float4(AccumulatedColor, 1.0f);
}