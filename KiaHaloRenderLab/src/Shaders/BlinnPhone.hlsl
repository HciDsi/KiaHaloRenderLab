#include "Util/MathUtil.hlsl"
#include "Util/LightUtil.hlsl"


// Constant data that varies per frame.

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    
    float4 gAmbientLight;

    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float3 posW : POSITION;
    float3 normalW : NORMAL;
};

VertexOut VS(VertexIn v)
{
    float4 posW = mul(float4(v.pos, 1.0f), gWorld);
    
    VertexOut o;
    o.posH = mul(posW, gViewProj);
    o.posW = posW.xyz;
    o.normalW = mul(v.normal, (float3x3) gWorld);
    
    return o;
}

float4 PS(VertexOut o) : SV_Target
{
    float3 V = normalize(gEyePosW - o.posW);
    float3 N = normalize(o.normalW);
    float3 L = normalize(-gLights[0].Dir);
    float3 H = normalize(V + L);
    
    float3 albedo = gAmbientLight.xyz * gDiffuseAlbedo.xyz;
    float3 diffuse = float3(0, 0, 0);
    float3 specular = float3(0, 0, 0);

    float m = gRoughness * 256.0f;
    float3 F = gFresnelR0 + (1 - gFresnelR0) * pow(1 - saturate(dot(N, L)), 5);

    diffuse += gLights[0].Strength * gDiffuseAlbedo.xyz * pow(saturate(dot(N, L)) * 0.5 + 0.5, 2);
    
    specular += F * gLights[0].Strength * pow(saturate(dot(N, H)), m);
    
    float3 lr = albedo + diffuse + specular;
    
    lr /= (lr + 1.0);
    lr = pow(lr, 1.0 / 2.2);
    
    return float4(lr, 1.0f);
}