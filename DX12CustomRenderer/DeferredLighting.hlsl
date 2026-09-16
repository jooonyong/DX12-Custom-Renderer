Texture2D GBufferA : register(t0);
SamplerState PointSampler : register(s0);

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

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
    return GBufferA.Sample(PointSampler, Input.UV);
}