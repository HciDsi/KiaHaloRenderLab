#include "RenderObjects.h"

//++++++++++++++++++++++++++Render Item++++++++++++++++++++++++++
RenderItem::RenderItem()
{
}

RenderItem::~RenderItem()
{
}

void RenderItem::SetTranslation(float t[])
{
	for (int i = 0; i < 3; i++)
		translation[i] = t[i];
}
void RenderItem::SetRotation(float r[])
{
	for (int i = 0; i < 3; i++)
		rotation[i] = r[i];
}
void RenderItem::SetScale(float s[])
{
	for (int i = 0; i < 3; i++)
		scale[i] = s[i];
}
void RenderItem::SetTranslation(float x, float y, float z)
{
	translation[0] = x;
	translation[1] = y;
	translation[2] = z;
}
void RenderItem::SetRotation(float x, float y, float z)
{
	rotation[0] = x;
	rotation[1] = y;
	rotation[2] = z;
}
void RenderItem::SetScale(float x, float y, float z)
{
	scale[0] = x;
	scale[1] = y;
	scale[2] = z;
}

DirectX::XMFLOAT4X4 RenderItem::GetWorld()
{
	DirectX::XMStoreFloat4x4(
		&World,
		DirectX::XMMatrixScaling(scale[0], scale[1], scale[2]) *
		DirectX::XMMatrixRotationX(rotation[0]) * 
		DirectX::XMMatrixRotationY(rotation[1]) * 
		DirectX::XMMatrixRotationZ(rotation[2]) *
		DirectX::XMMatrixTranslation(translation[0], translation[1], translation[2])
	);

	return World;
}

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
void Material::SetFresnelR0(float r)
{
	FresnelR0.x = r;
	FresnelR0.y = r;
	FresnelR0.z = r;
}
void Material::SetRoughness(float r)
{
	Roughness = r;
}

//++++++++++++++++++++++++++Frame Resouce++++++++++++++++++++++++++
FrameRecouce::FrameRecouce(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
	ThrowIfFailed(
		device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&cmdAlloc)
		)
	);

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	ObjCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
}

FrameRecouce::~FrameRecouce()
{
}