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
    float gMatellic;
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
    float3 N = normalize(o.normalW);
    float3 V = normalize(gEyePosW - o.posW);
    
    float3 F0 = gFresnelR0;
    F0 = lerp(F0, gDiffuseAlbedo.xyz, gMatellic);
    
    float3 c = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < 1; i++)
    {
        float3 L = normalize(-gLights[i].Dir);
        float3 H = normalize(V + L);
    
        float NDotH = saturate(dot(N, H));
        float NDotL = saturate(dot(N, L));
        float NDotV = saturate(dot(N, V));
        float LDotH = saturate(dot(L, H));
    
        float D = GGX(NDotH, gRoughness);
        float3 F = SchlickFresnel(NDotH, F0);
        float G = min(
            1.0,
            min(2.0 * NDotH * NDotV / LDotH, 2.0 * NDotH * NDotL / LDotH)
        );
        
        float3 kd = 1.0 - F;
        kd *= 1.0 - gMatellic;
    
        float3 albedo = gAmbientLight.xyz * gDiffuseAlbedo.xyz;
        float3 spcular = D * F * G / (4.0 * NDotL * NDotV + 0.001);
        float3 diffuse = kd * gDiffuseAlbedo.xyz / m_PI;
        
        c += (spcular + diffuse) * NDotL * gLights[i].Strength;
    }
    
    float3 ambient = 0.1 * gDiffuseAlbedo.xyz;
    c += ambient;
    
    c = c / (c + 1.0f);
    c = pow(c, 1.0 / 2.2);
    
    return float4(c, 1.0f);
}