struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

cbuffer TransformBuffer : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
}

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;
    
    Output.Position = mul(float4(Input.Position, 1.0f), World);  
    Output.Position = mul(Output.Position, View);
    Output.Position = mul(Output.Position, Projection);

    Output.Color = Input.Color;

    return Output;
}

float4 PSMain(VSOutput Input) : SV_TARGET
{
    return Input.Color;
}