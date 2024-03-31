
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