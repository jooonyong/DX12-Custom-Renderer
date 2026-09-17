Texture2D GBufferA : register(t0);
Texture2D GBufferB : register(t1);
Texture2D GBufferC : register(t2);
Texture2D SceneDepth : register(t3);
Texture2D ShadowMapTexture : register(t4);

SamplerState PointSampler : register(s0);
SamplerState ShadowMapSampler : register(s1);

static const float PI = 3.1415926535;

cbuffer InverseViewMatrix : register(b0)
{
    float4x4 InverseViewMatrix;
    float3 CameraPosition;
    float Padding;
}

cbuffer DirLgtBuffer : register(b1)
{
    float3 LightDirection;
    float LightIntensity;
    float3 LightColor;
    float AmbientIntensity;
}

cbuffer ShadowPassConstant : register(b2)
{
    float4x4 LightViewProjectionMatrix;
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

//Cook-Torrance 반사 모델을 위한 함수
//Specular BRDF = (D * F * G) / (4 * N·L * N·V)
//D: Normal Distribution Function
//F: Fresnel
//G: Geometry Shade masking
float DistributionGGX(float3 N, float3 H, float Roughness)
{
    float a2 = Roughness * Roughness * Roughness * Roughness;
    float Distribution = (dot(N, H) * a2 - dot(N, H)) * dot(N, H) + 1;

    return a2 / (PI * Distribution * Distribution);
}

float GeometrySchlickGGX(float NdotX, float Roughness)
{
    float R = Roughness + 1.0f;
    float K = (R * R) / 8.0f;

    return NdotX / (NdotX * (1.0f - K) + K);
}

float GeometrySmith(float3 N, float3 V, float3 L, float Roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    float GGXV = GeometrySchlickGGX(NdotV, Roughness);
    float GGXL = GeometrySchlickGGX(NdotL, Roughness);

    return GGXV * GGXL;
}

float3 FresnelSchlick(float CosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - CosTheta, 5.0f);
}


float3 LinearToSRGB(float3 Color)
{
    float3 Low = Color * 12.92f;

    float3 High = 1.055f * pow(max(Color, 0.0f), 1.0f / 2.4f) - 0.055f;

    return lerp(Low, High, step(0.0031308f, Color));
}


float CalculateShadowFactor(float3 WorldPosition)
{
    //Shadow 계산
    float4 ShadowPosition = mul(float4(WorldPosition, 1.0f), LightViewProjectionMatrix);

    float3 ShadowNDC = ShadowPosition.xyz / ShadowPosition.w;
    float2 ShadowUV;
    ShadowUV.x = ShadowNDC.x * 0.5f + 0.5f;
    ShadowUV.y = -ShadowNDC.y * 0.5f + 0.5f; //y축과 v축이 반대이므로 -1을 곱해줌

    float CurrentDepth = ShadowNDC.z;

    uint Width;
    uint Height;
    ShadowMapTexture.GetDimensions(Width, Height);

    float2 TexelSize = 1.0f / float2(Width, Height);

    float ShadowBias = 0.001f;
    float LitCount = 0.0f;

    //3*3 PCF
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float2 Offset = float2(x, y) * TexelSize;
            float StoredDepth = ShadowMapTexture.SampleLevel(ShadowMapSampler, ShadowUV + Offset, 0).r;
            if (StoredDepth + ShadowBias < CurrentDepth)
            {
                //LitCount += 0.0f; //Shadow
            }
            else
            {
                LitCount += 1.0f; //No Shadow
            }
        }
    }

    return LitCount / 9.0f;
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
    float4 BaseColorData = GBufferA.Sample(PointSampler, Input.UV);
    float4 NormalData = GBufferB.Sample(PointSampler, Input.UV);
    float4 MaterialData = GBufferC.Sample(PointSampler, Input.UV);
   
    float Depth = SceneDepth.Sample(PointSampler, Input.UV).r;
    if (Depth >= 0.999999f)
    {
        return float4(0.0f, 0.2f, 0.4f, 1.0f);
    }
    
    float3 WorldPosition = ReconstructWorldPosition(Input.UV, Depth);
    float ShadowFactor = CalculateShadowFactor(WorldPosition);

    float3 Albedo = BaseColorData.rgb;

    float3 N = normalize(NormalData.xyz);

    float Roughness = MaterialData.r;
    float Metallic = MaterialData.g;

    float3 V = normalize(CameraPosition - WorldPosition);
    float3 L = normalize(-LightDirection);
    float3 H = normalize(V + L);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, Albedo, Metallic);

    float D = DistributionGGX(N, H, Roughness);

    float G =  GeometrySmith(N,V, L, Roughness);

    float3 F = FresnelSchlick( saturate(dot(H, V)), F0);
    float3 Numerator = D * G * F;

    float Denominator = max( 4.0f * NdotV * NdotL, 0.001f);

    float3 Specular = Numerator / Denominator;
   
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - Metallic);

    float3 Diffuse = kD * Albedo / PI;
    float3 Radiance = LightColor * LightIntensity;
    float3 DirectLighting = (Diffuse + Specular) * Radiance * NdotL;
    DirectLighting *= ShadowFactor;

    float3 Ambient = Albedo * AmbientIntensity;

    float3 FinalColor = DirectLighting + Ambient;
    FinalColor = LinearToSRGB(FinalColor);

    return float4(ShadowFactor, ShadowFactor, ShadowFactor, 1.0f);
    //return float4(FinalColor, 1.0f);
}