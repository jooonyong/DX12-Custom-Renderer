Texture2D AlbedoTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;
};

cbuffer TransformBuffer : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
}

cbuffer MaterialBuffer : register(b1)
{
    float4 BaseColor;
    float Roughness;
    float Metallic;
    float2 MaterialPadding;
}

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;
    
    Output.Position = mul(float4(Input.Position, 1.0f), World);  
    Output.Position = mul(Output.Position, View);
    Output.Position = mul(Output.Position, Projection);

    Output.Color = Input.Color;
    Output.UV = Input.UV;

    return Output;
}

float4 PSMain(VSOutput Input) : SV_TARGET
{
    //return AlbedoTexture.Sample(LinearSampler, Input.UV);
    float4 Albedo = AlbedoTexture.Sample(LinearSampler, Input.UV);

    return Albedo * BaseColor;
}