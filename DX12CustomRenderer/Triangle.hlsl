Texture2D AlbedoTexture : register(t0);
Texture2D MetallicRoughnessTexture : register(t1);
Texture2D NormalMapTexture : register(t2);
SamplerState LinearSampler : register(s0);

static const float PI = 3.1415926535;

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;
    float3 Normal   : NORMAL;
    float4 Tangent  : Tangent;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD;
    
    float3 WorldNormal   : TEXCOORD1;
    float4 WorldTangent  : TEXCOORD2;
    float3 WorldPosition : TEXCOORD3;
};

cbuffer TransformBuffer : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
    float4x4 WorldInverseTranspose;

    float3 CameraPosition;
    float Padding;
}

cbuffer MaterialBuffer : register(b1)
{
    float4 BaseColor;
    float Roughness;
    float Metallic;
    float2 MaterialPadding;
}

cbuffer DirLgtBuffer : register(b2)
{
    float3 LightDirection;
    float LightIntensity;
    float3 LightColor;
    float AmbientIntensity;
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

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;
    
    float3 WorldNormal = mul(float4(Input.Normal, 0.0f), WorldInverseTranspose).xyz;
    float3 T = mul(float4(Input.Tangent.xyz, 0.0f), World).xyz;

    Output.Position = mul(float4(Input.Position, 1.0f), World);  
    Output.Position = mul(Output.Position, View);
    Output.Position = mul(Output.Position, Projection);

    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;

    Output.UV = Input.UV;
    Output.WorldNormal = normalize(WorldNormal);
    Output.WorldTangent = float4(normalize(T), Input.Tangent.w);

    return Output;
}

float4 PSMain(VSOutput Input) : SV_TARGET
{
    float3 N = normalize(Input.WorldNormal);  //Normal
    float3 T = normalize(Input.WorldTangent.xyz); //Tangent for TangentSpace
    float3 L = normalize(-LightDirection); //Light Direction
    float3 V = normalize(CameraPosition - Input.WorldPosition.xyz); //View Direction
   
    //그램슈미트 직교화
    T = normalize(T - N * dot(T, N)); 
    float3 B = normalize(cross(N, T)) * Input.WorldTangent.w;  //BiTangent
    float3 NormalTS = NormalMapTexture.Sample(LinearSampler, Input.UV).xyz;
    NormalTS = NormalTS * 2.0f - 1.0f;
    
    float3 NormalWS = normalize(NormalTS.x * T + NormalTS.y * B + NormalTS.z * N);
    N = NormalWS;

    //specular
    float3 H = normalize(L + V); //LightDir vector + ViewDir Vector

    //Diffuse
    float NdotL = max(dot(N,L), 0.0f);
    //Specular(Normal과 H가 평행일수록 specular가 강해짐
    float NdotH = max(dot(N, H), 0.0f);
    float NdotV = max(dot(N, V), 0.0f);

    float4 TextureColor = AlbedoTexture.Sample(LinearSampler, Input.UV);
    float3 Albedo = TextureColor.rgb * BaseColor.rgb;

    float4 MR = MetallicRoughnessTexture.Sample(LinearSampler, Input.UV);
    float MaterialRoughness = clamp(MR.g * Roughness, 0.04f, 1.0f);
    float MaterialMetallic = saturate(MR.b * Metallic);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo, MaterialMetallic);

    float D = DistributionGGX(N, H, MaterialRoughness);
    float3 F = FresnelSchlick(saturate(dot(H, V)), F0);
    float G = GeometrySmith(N, V, L, MaterialRoughness);

    float3 Numerator = D * G * F;
    float Denominator = 4.0f * NdotV * NdotL;

    float3 Specular = Numerator / max(Denominator, 0.0001f);
    //float3 Diffuse = Albedo * LightColor * LightIntensity * NdotL;
   
    float3 KD = 1.0f - F;
    KD *= (1.0f - MaterialMetallic);

    float3 DiffuseBRDF = KD * Albedo / PI;
    
    float3 Radiance = LightColor * LightIntensity;
    float3 DirectLighting = (DiffuseBRDF + Specular) * Radiance * NdotL;
    float3 Ambient = 0;
    
    float3 FinalColor = DirectLighting;

    float3 DisplayColor = LinearToSRGB(saturate(FinalColor));

    return float4(DisplayColor, TextureColor.a * BaseColor.a);
}