cbuffer ShadowObjectBuffer : register(b0)
{
    float4x4 World;
}

cbuffer ShadowPassBuffer : register(b1)
{
    float4x4 LightViewProjection;
}

struct VSInput
{
    float3 Position : POSITION;
};

float4 VSMain(VSInput Input) : SV_POSITION
{
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);

    return mul(WorldPosition, LightViewProjection);
}