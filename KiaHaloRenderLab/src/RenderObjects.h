#pragma once

#include "Dx12Render/MathHelper.h"
#include "Dx12Render/UploadBuffer.h"

//++++++++++++++++++++++++++Vertex Date++++++++++++++++++++++++++
struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
};

//++++++++++++++++++++++++++Light++++++++++++++++++++++++++
#define MaxLights 16
struct Light 
{
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;                          // point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
	float FalloffEnd = 10.0f;                           // point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
	float SpotPower = 64.0f;                            // spot light only
};

//++++++++++++++++++++++++++Constant Buffer++++++++++++++++++++++++++
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();

	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	Light Lights[MaxLights];
};

struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

//++++++++++++++++++++++++++Material++++++++++++++++++++++++++
class Material
{
public:
	Material();
	~Material();

	void SetDiffuseAlbedo(float r, float g, float b, float a);
	void SetFresnelR0(float r);
	void SetRoughness(float r);

	std::string Name;

	int MatCBIndex = -1;

	int DiffuseSrvHeapIndex = -1;

	int NormalSrvHeapIndex = -1;

	int NumFramesDirty = 0;

	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = .25f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

//++++++++++++++++++++++++++Render Item++++++++++++++++++++++++++
class RenderItem
{
public:

	RenderItem();
	~RenderItem();

	void SetTranslation(float t[]);
	void SetRotation(float r[]);
	void SetScale(float s[]);
	void SetTranslation(float x, float y, float z);
	void SetRotation(float x, float y, float z);
	void SetScale(float x, float y, float z);



	DirectX::XMFLOAT4X4 GetWorld();

	std::string Name;

	float translation[3] = { 0.0f, 0.0f, 0.0f };
	float rotation[3] = { 0.0f, 0.0f, 0.0f };
	float scale[3] = { 1.0f, 1.0f, 1.0f };

	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	UINT NumFramesDirty = 0;

	UINT ObjCBIndex = -1;

	MeshGeometry* Geo;
	Material* Mat;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	int IndexCount = 0;
	int StartIndex = 0;
	int StartVertex = 0;
};

//++++++++++++++++++++++++++Frame Resouce++++++++++++++++++++++++++
class  FrameRecouce
{
public:
	 FrameRecouce(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	~ FrameRecouce();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
	
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjCB;
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB;

	UINT64 Fence = 0;
};