Texture2D GBufferA : register(t0);
Texture2D GBufferB : register(t1);
Texture2D GBufferC : register(t2);
Texture2D SceneDepth : register(t3);

SamplerState PointSampler : register(s0);

cbuffer InverseViewMatrix : register(b0)
{
    float4x4 InverseViewMatrix;
    float3 CameraPosition;
    float Padding;
}

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

float3 ReconstructWorldPosition(float2 UV, float Depth)
{
    float2 NDC;

    NDC.x = UV.x * 2.0f - 1.0f;
    NDC.y = 1.0f - UV.y * 2.0f;

    float4 Position = float4(NDC.x, NDC.y, Depth, 1.0f);
    float4 WorldPosition = mul(Position, InverseViewMatrix);

    WorldPosition.xyz /= WorldPosition.w;

    return WorldPosition.xyz;
}

VSOutput VSMain(uint VertexID : SV_VertexID)
{
    VSOutput Output;
    static const float2 Positions[3] =
    {
        float2(-1.0f,  1.0f),
        float2( 3.0f,  1.0f),
        float2(-1.0f, -3.0f)
    };

    static const float2 UVs[3] =
    {
        float2(0.0f, 0.0f),
        float2(2.0f, 0.0f),
        float2(0.0f, 2.0f)
    };

    Output.Position = float4(Positions[VertexID], 0.0f, 1.0f);
    Output.UV = UVs[VertexID];

    return Output;
}


float4 PSMain(VSOutput Input) : SV_TARGET0
{
    float3 WorldPosition = ReconstructWorldPosition(Input.UV, SceneDepth.Sample(PointSampler, Input.UV));
    return float4(WorldPosition, 1.0f);
    //return GBufferA.Sample(PointSampler, Input.UV);
}