RaytracingAccelerationStructure SceneAS : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

Texture2D<float4> BaseColorTexture : register(t3);
Texture2D<float4> MetallicRoughnessTexture : register(t4);
Texture2D<float4> NormalTexture : register(t5);

SamplerState MaterialSampler : register(s0);

static const float PI = 3.1415926535f;

cbuffer InverseViewMatrix : register(b0)
{
    float4x4 InverseViewMatrix;
    float3 CameraPosition;
    float Padding;
}

cbuffer DirectionalLightConstant : register(b1)
{
    float3 Direction;
    float Intensity;
    float3 Color;
    float AmbientIntensity;
}

cbuffer MaterialConstants : register(b2)
{
    float4 BaseColor;
    float Roughness;
    float Metallic;
    float2 MaterialPadding;
};

struct Vertex
{
    float3 Position;
    float4 Color;
    float2 UV;
    float3 Normal;
    float4 Tangent;
};

StructuredBuffer<Vertex> Vertices : register(t1);
ByteAddressBuffer Indices : register(t2);

struct RayPayload
{
	float3 Color;
};

struct Attributes
{
	float2 Barycentrics;
};


float DistributionGGX(float3 N, float3 H, float Roughness)
{
    float a2 = Roughness * Roughness * Roughness * Roughness;
    float NdotH = saturate(dot(N, H));
    float Denominator = NdotH * NdotH * (a2 - 1.0f) + 1.0f;

    return a2 / (PI * Denominator * Denominator);
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

[shader("raygeneration")]
void RayGen()
{
	uint2 Index = DispatchRaysIndex().xy;
	uint2 Dimensions = DispatchRaysDimensions().xy;

	float2 UV = (float2(Index) + 0.5f) / float2(Dimensions);
	float2 NDC = UV * 2.0f - 1.0f;

	NDC.y = -NDC.y;

    float4 NearPlane = mul(float4(NDC.x, NDC.y, 0.0f, 1.0f), InverseViewMatrix);
    float4 FarPlane = mul(float4(NDC.x, NDC.y, 1.0f, 1.0f), InverseViewMatrix);
    NearPlane.xyz = NearPlane.xyz / NearPlane.w;
    FarPlane.xyz = FarPlane.xyz / FarPlane.w;
    
    RayDesc Ray;
    Ray.Origin = CameraPosition;
    Ray.Direction = normalize(FarPlane.xyz - CameraPosition);

    Ray.TMin = 0.001f;
    Ray.TMax = 10000.0f;

    RayPayload Payload;
    Payload.Color = float3(0.0f, 0.0f, 0.0f);

    TraceRay(SceneAS, RAY_FLAG_NONE, 0xFF, 0, 1, 0, Ray, Payload);

    OutputTexture[Index] = float4(Payload.Color, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload Payload)
{
    Payload.Color = float3(0.1f, 0.2f, 0.4f);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload Payload, in BuiltInTriangleIntersectionAttributes Attr)
{
    uint Primitive = PrimitiveIndex();
    uint3 TriangleIndices = Indices.Load3(Primitive * 12);
    
    Vertex V0 = Vertices[TriangleIndices.x];
    Vertex V1 = Vertices[TriangleIndices.y];
    Vertex V2 = Vertices[TriangleIndices.z];
    
    float3 Barycentrics;
    Barycentrics.yz = Attr.barycentrics;
    Barycentrics.x = 1 - Barycentrics.y - Barycentrics.z;
    
    float2 UV = V0.UV * Barycentrics.x + V1.UV * Barycentrics.y + V2.UV * Barycentrics.z;
    float4 BaseColorSample = BaseColorTexture.SampleLevel(MaterialSampler,UV, 0.0f);
    float4 MRSample = MetallicRoughnessTexture.SampleLevel(MaterialSampler, UV, 0.0f);
    
    float3 ObjectNormal = normalize(V0.Normal * Barycentrics.x + V1.Normal * Barycentrics.y + V2.Normal * Barycentrics.z);
    float3 WorldNormal = normalize(mul(ObjectNormal, (float3x3) transpose(WorldToObject3x4())));
    
    float3 WorldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 L = normalize(-Direction);
    float3 V = normalize(CameraPosition - WorldPosition);
    float3 H = normalize(V + L);
    
    float NDotL = saturate(dot(WorldNormal, L));
    float NDotV = saturate(dot(WorldNormal, V));
    
    float MaterialRoughness = max(Roughness * MRSample.g, 0.04f);
    float MaterialMetallic = saturate(Metallic * MRSample.b);
    
    float3 Albedo = BaseColor.rgb * BaseColorSample.rgb;
    
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, Albedo, MaterialMetallic);

    float D = DistributionGGX(WorldNormal, H, MaterialRoughness);
    float G = GeometrySmith(WorldNormal, V, L, MaterialRoughness);
    float3 F = FresnelSchlick(saturate(dot(H, V)), F0);
    
    float3 Numerator = D * G * F;

    float Denominator = max(4.0f * NDotV * NDotL, 0.001f);
    
    float3 Specular = Numerator / Denominator;
       
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - Metallic);

    float3 Diffuse = kD * Albedo / PI;
    float3 Radiance = Color * Intensity;
    float3 DirectLighting = (Diffuse + Specular) * Radiance * NDotL;
    
    float3 Ambient = Albedo * AmbientIntensity;
    
    Payload.Color = Ambient + DirectLighting;
}