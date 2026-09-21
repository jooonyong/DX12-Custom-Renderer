Texture2D SceneColorTexture : register(t0);
SamplerState PointSampler : register(s0);

cbuffer ToneMappingConstants : register(b0)
{
    float Exposure;
}

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

//ACES Tonemapping Approximation Function
float3 ACESFilm(float3 Color)
{
    float A = 2.51f;
    float B = 0.03f;
    float C = 2.43f;
    float D = 0.59f;
    float E = 0.14f;

    return clamp((Color * (A * Color + B)) / (Color * (C * Color + D) + E), 0.0f, 1.0f);
}


float3 LinearToSRGB(float3 Color)
{
    float3 Low = Color * 12.92f;
    float3 High = 1.055f * pow(max(Color, 0.0f), 1.0f / 2.4f) - 0.055f;

    return lerp(Low, High, step(0.0031308f, Color));
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
    float4 Color = SceneColorTexture.Sample(PointSampler, Input.UV);
    Color *= Exposure;
    
    Color.rgb = ACESFilm(Color.rgb);
    float3 FinalColor = LinearToSRGB(Color.rgb);

    return float4(FinalColor, 1.0f);
}