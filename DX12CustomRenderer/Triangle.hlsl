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

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;

    Output.Position = float4(Input.Position, 1.0f);
    Output.Color = Input.Color;

    return Output;
}

float4 PSMain(VSOutput Input) : SV_TARGET
{
    return Input.Color;
}