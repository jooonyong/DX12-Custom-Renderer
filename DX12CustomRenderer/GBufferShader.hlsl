Texture2D AlbedoTexture : register(t0);
Texture2D MetallicRoughnessTexture : register(t1);
Texture2D NormalMapTexture : register(t2);

SamplerState LinearSampler : register(s0);

cbuffer ObjectBuffer : register(b0)
{
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4x4 PreviousWorld;
}

cbuffer SceneBuffer : register(b1)
{
    float4x4 View;
    float4x4 Projection;
    float4x4 PreviousViewProjection;
    float HistoryValid;
    float3 CameraPosition;
    float2 RenderTargetSize;
    float2 Padding;
}
cbuffer MaterialBuffer : register(b2)
{
    float4 BaseColor;
    float Roughness;
    float Metallic;
    float2 MaterialPadding;
}

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
    float4 PrevClipPosition : TEXCOORD3;
};

struct GBufferOutput
{
    float4 GBufferA : SV_Target0;
    float4 GBufferB : SV_Target1;
    float4 GBufferC : SV_Target2;
    float4 GBufferVelocity : SV_Target3;
};

VSOutput VSMain(VSInput Input)
{
	VSOutput Output;
    
    float3 WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
    float3 WorldNormal = mul(float4(Input.Normal, 0.0f), WorldInverseTranspose).xyz;
    float3 T = mul(float4(Input.Tangent.xyz, 0.0f), World).xyz;
  
    Output.Position = mul(float4(WorldPosition, 1.0f), View);
    Output.Position = mul(Output.Position, Projection);
    
    float4 PreviousWorldPosition = mul(float4(Input.Position, 1.0f), PreviousWorld);
    float4 PreviousClipPosition = mul(PreviousWorldPosition, PreviousViewProjection);
    
    Output.UV = Input.UV;
    Output.WorldNormal = normalize(WorldNormal);
    Output.WorldTangent = float4(T, Input.Tangent.w);
    Output.PrevClipPosition = PreviousClipPosition;
    
    return Output;
}

GBufferOutput PSMain(VSOutput Input)
{
    GBufferOutput Output;

    float4 TextureColor = AlbedoTexture.Sample(LinearSampler, Input.UV);
    float3 Albedo = TextureColor.rgb * BaseColor.rgb;
    
    //BaseColor
    Output.GBufferA = float4(Albedo, TextureColor.a * BaseColor.a);
    
    //Normal
    float3 N = normalize(Input.WorldNormal);  //Normal
    float3 T = normalize(Input.WorldTangent.xyz); //Tangent for TangentSpace
    //float3 V = normalize(CameraPosition - Input.WorldPosition.xyz); //View Direction
   
    //그램슈미트 직교화
    T = normalize(T - N * dot(T, N)); 

    float3 B = normalize(cross(N, T)) * Input.WorldTangent.w;  //BiTangent
    float3 NormalTS = NormalMapTexture.Sample(LinearSampler, Input.UV).xyz;

    //노멀맵 [0,1] to [-1,1]
    NormalTS = NormalTS * 2.0f - 1.0f;
    
    float3 NormalWS = normalize(NormalTS.x * T + NormalTS.y * B + NormalTS.z * N);
    N = NormalWS;
    Output.GBufferB = float4(N, 1.0f);
    
    //MaterialMetallic, MaterialRoughness
    float4 MR = MetallicRoughnessTexture.Sample(LinearSampler, Input.UV);
    float MaterialRoughness = clamp(MR.g * Roughness, 0.04f, 1.0f);
    float MaterialMetallic = saturate(MR.b * Metallic);
    Output.GBufferC = float4(MaterialRoughness, MaterialMetallic, 0.0f, 1.0f);

    //MotionVector
    
    float2 CurrentUV = Input.Position.xy / RenderTargetSize;
    float2 PrevNDC = Input.PrevClipPosition.xy / Input.PrevClipPosition.w;

    float2 PrevUV;
    PrevUV.x = PrevNDC.x * 0.5f + 0.5f;
    PrevUV.y = 0.5f - PrevNDC.y * 0.5f;
    
    float4 MotionVector = CurrentUV - PrevUV;
    Output.GBufferVelocity = float4(MotionVector.x, MotionVector.y, HistoryValid, 0.0f);
    return Output;
}