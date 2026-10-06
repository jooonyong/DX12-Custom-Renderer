RaytracingAccelerationStructure SceneAS : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

struct RayPayload
{
	float3 Color;
};

struct Attributes
{
	float2 Barycentrics;
};

[shader("raygeneration")]
void RayGen()
{
	uint2 Index = DispatchRaysIndex().xy;
	uint2 Dimensions = DispatchRaysDimensions().xy;

	float2 UV = (float2(Index) + 0.5f) / float2(Dimensions);
	float2 NDC = UV * 2.0f - 1.0f;

	NDC.y = -NDC.y;

    RayDesc Ray;
    Ray.Origin = float3(0.0f, 0.0f, -5.0f);
    Ray.Direction = normalize(float3(NDC.x, NDC.y, 1.0f));

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
    Payload.Color = float3(1.0f, 0.0f, 0.0f);
}