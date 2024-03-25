#include "RenderObjects.h"

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
}

FrameRecouce::~FrameRecouce()
{
}