#include "Materials.h"

//++++++++++++++++++++++++++Material++++++++++++++++++++++++++
Material::Material()
{
}

Material::~Material()
{
}

void Material::SetDiffuseAlbedo(float r, float g, float b, float a)
{
	DiffuseAlbedo.x = r;
	DiffuseAlbedo.y = g;
	DiffuseAlbedo.z = b;
	DiffuseAlbedo.w = a;
}
void Material::SetFresnelR0(float r[])
{
	FresnelR0.x = r[0];
	FresnelR0.y = r[1];
	FresnelR0.z = r[2];
}
void Material::SetRoughness(float r)
{
	Roughness = r;
}

void Material::SetMatellic(float m)
{
	Matellic = m;
}