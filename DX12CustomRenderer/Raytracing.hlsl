RaytracingAccelerationStructure SceneAS : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

Texture2D<float4> BaseColorTexture : register(t3);
Texture2D<float4> MetallicRoughnessTexture : register(t4);
Texture2D<float4> NormalTexture : register(t5);

Texture2D<float4> HybridGBufferA : register(t6);
Texture2D<float4> HybridGBufferB : register(t7);
Texture2D<float4> HybridGBufferC : register(t8);
Texture2D<float> HybridSceneDepth : register(t9);

RWTexture2D<float> RaytracedShadow : register(u1);
RWTexture2D<float4> RaytracedReflection : register(u2);

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

struct ShadowPayload
{
    uint Occluded;
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

    //RecordIndex = InstanceOffset + GeometryIndex * GeometryMultiplier + RayOffset
    TraceRay(SceneAS, RAY_FLAG_NONE, 0xFF, 0, 2, 0, Ray, Payload); //HitOffset 0, GeometryMultiplier 2, MissshaderIndex 0

    OutputTexture[Index] = float4(Payload.Color, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload Payload)
{
    Payload.Color = float3(1.0f, 0.2f, 0.4f);
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
    
    float3 ObjectTangent = V0.Tangent.xyz * Barycentrics.x + V1.Tangent.xyz * Barycentrics.y + V2.Tangent.xyz * Barycentrics.z;
    float3 WorldTangent = mul((float3x3) ObjectToWorld3x4(), ObjectTangent);
    
    //그램슈미트 직교화
    WorldTangent = normalize(WorldTangent - WorldNormal * dot(WorldTangent, WorldNormal));
    float3 Bitangent = normalize(cross(WorldNormal, WorldTangent)) * V0.Tangent.w;
    float3 NormalSample = NormalTexture.SampleLevel(MaterialSampler, UV, 0.0f).xyz;
    NormalSample = NormalSample * 2.0f - 1.0f;
    
    float3 NormalWS = normalize(NormalSample.x * WorldTangent + NormalSample.y * Bitangent + NormalSample.z * WorldNormal);
    WorldNormal = NormalWS;
    
    float3 WorldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    
    float3 L = normalize(-Direction);
    float3 V = normalize(CameraPosition - WorldPosition);
    float3 H = normalize(V + L);
    
    float3 WorldEdge1 = mul((float3x3) ObjectToWorld3x4(), V1.Position - V0.Position);
    float3 WorldEdge2 = mul((float3x3) ObjectToWorld3x4(), V2.Position - V0.Position);
    float3 WorldGeometricNormal = normalize(cross(WorldEdge1, WorldEdge2));
    
    float3 OffsetNormal = dot(WorldGeometricNormal, L) > 0 ? WorldGeometricNormal : -WorldGeometricNormal;
    float ShadowBias = 0.001f;
    float3 ShadowOrigin = WorldPosition + ShadowBias * OffsetNormal;
     
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
    float3 kD = (1.0f - kS) * (1.0f - MaterialMetallic);

    float3 Diffuse = kD * Albedo / PI;
    float3 Radiance = Color * Intensity;
    float3 DirectLighting = (Diffuse + Specular) * Radiance * NDotL;
    
    float3 Ambient = Albedo * AmbientIntensity;
    
    RayDesc ShadowRayDesc;
    ShadowRayDesc.Direction = L;
    ShadowRayDesc.Origin = ShadowOrigin;
    ShadowRayDesc.TMin = 0.001f;
    ShadowRayDesc.TMax = 10000.0f;
    
    ShadowPayload ShadowInfo;
    ShadowInfo.Occluded = 0;
    
    TraceRay(SceneAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 1, 2, 1, ShadowRayDesc, ShadowInfo);
    
    Payload.Color = Ambient + DirectLighting * (1 - ShadowInfo.Occluded);
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowPayload Payload, in BuiltInTriangleIntersectionAttributes Attr)
{
    Payload.Occluded = 1;
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload Payload)
{
    Payload.Occluded = 0;
}

[shader("raygeneration")]
void ShadowRayGen()
{
    uint2 Pixel = DispatchRaysIndex().xy;
    uint2 Dimensions = DispatchRaysDimensions().xy;

    float Depth = HybridSceneDepth.Load(int3(Pixel, 0));
    
    // 배경이면 RaytracedShadow[Pixel]에 1 기록
    if(Depth == 1.0f)
    {
        RaytracedShadow[Pixel] = 1.0f;
        return;
    }
    
    float2 UV = (float2(Pixel) + 0.5f) / float2(Dimensions);
    float2 NDC = UV * 2.0f - 1.0f;
    NDC.y = -NDC.y;

    //WorldPosition 복원
    float4 Position = float4(NDC.x, NDC.y, Depth, 1.0f);
    float4 WorldPosition = mul(Position, InverseViewMatrix);

    WorldPosition.xyz /= WorldPosition.w;
    
    float3 L = normalize(-Direction);
    float ShadowBias = 0.001f;
    float3 ShadowOrigin = WorldPosition.xyz + ShadowBias * L;
    
    RayDesc ShadowRay;
    ShadowRay.Direction = L;
    ShadowRay.Origin = ShadowOrigin;
    ShadowRay.TMin = 0.001f;
    ShadowRay.TMax = 10000.0f;
    
    ShadowPayload ShadowInfo;
    ShadowInfo.Occluded = 0;
    
    TraceRay(SceneAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 1, 2, 1, ShadowRay, ShadowInfo);
    
    RaytracedShadow[Pixel] = 1 - ShadowInfo.Occluded;
}

[shader("raygeneration")]
void ReflectionRayGen()
{
    uint2 Pixel = DispatchRaysIndex().xy;
    RaytracedReflection[Pixel] = float4(1.0f, 1.0f, 0.1f, 1.0f);
}