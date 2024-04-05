
#define MaxLights 16

struct Light
{
    float3 Strength;
    float Start;
    float3 Dir;
    float End;
    float3 Pos;
    float SpotPower;
};

struct Material
{
    float4 DifuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

float3 SchlickFresnel(float NDotH, float3 F0)
{
    return F0 + (1 - F0) * Pow5(NDotH);
}

float GGX(float NDotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NDotH2 = NDotH * NDotH;
    
    float d = NDotH2 * (alpha2 - 1) + 1;
    
    return alpha2 / (m_PI * d * d);
}