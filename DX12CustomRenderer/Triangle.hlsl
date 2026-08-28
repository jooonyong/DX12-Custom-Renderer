Texture2D AlbedoTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;
    float3 Normal   : NORMAL;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;
    float3 Normal   : NORMAL;

    float3 WorldPosition : POSITION1;
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

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;
    
    float3 WorldNormal = mul(float4(Input.Normal, 0.0f), WorldInverseTranspose).xyz;

    Output.Position = mul(float4(Input.Position, 1.0f), World);  
    Output.Position = mul(Output.Position, View);
    Output.Position = mul(Output.Position, Projection);

    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;

    Output.Color = Input.Color;
    Output.UV = Input.UV;
    Output.Normal = normalize(WorldNormal);

    return Output;
}

float4 PSMain(VSOutput Input) : SV_TARGET
{
    float3 N = normalize(Input.Normal);  //Normal
    float3 L = normalize(-LightDirection); //Light Direction
    float3 V = normalize(CameraPosition - Input.WorldPosition.xyz); //View Direction
    //Blinn Phong specular
    float3 H = normalize(L + V); //LightDir vector + ViewDir Vector

    //Diffuse
    float NdotL = max(dot(N,L), 0.0f);
    //Specular(Normal과 H가 평행일수록 specular가 강해짐
    float NdotH = max(dot(N, H), 0.0f);

    float4 TextureColor = AlbedoTexture.Sample(LinearSampler, Input.UV);
    float3 Albedo = TextureColor.rgb * BaseColor.rgb;

    float3 Diffuse = Albedo * LightColor * LightIntensity * NdotL;
    float3 Ambient = Albedo * AmbientIntensity;
    
    float Shininess = lerp(128.0f, 4.0f, Roughness);
    float SpecularFactor = 0;
    if (NdotL > 0.0f)
    {
        SpecularFactor = pow(NdotH, Shininess);
    }
    
    float3 Specular = LightColor * LightIntensity * SpecularFactor;

    float3 FinalColor = Diffuse + Ambient + Specular;
    //return float4(Input.UV, 0.0f, 1.0f);
    return float4(FinalColor, TextureColor.a * BaseColor.a);
}