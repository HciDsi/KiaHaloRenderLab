#pragma once
#include "Dx12Render/d3dUtil.h"
#include "Dx12Render/MathHelper.h"
#include "Dx12Render/UploadBuffer.h"

//++++++++++++++++++++++++++Vertex Date++++++++++++++++++++++++++
struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
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
};

//++++++++++++++++++++++++++Render Item++++++++++++++++++++++++++
struct RenderItem
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	UINT NumFramesDirty = 0;

	UINT ObjCBIndex = -1;

	MeshGeometry* Geo;

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

	UINT64 Fence = 0;
};