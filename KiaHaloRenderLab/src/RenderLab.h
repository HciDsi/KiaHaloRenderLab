#pragma once

#include "Dx12Render/MathHelper.h"
#include "Dx12Render/UploadBuffer.h"
#include "Dx12Render/D3DApp.h"
#include "ImGui/KiaHaloUI.h"
#include "RenderObjects.h"
#include <DirectXColors.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
};

class RenderLab : public D3DApp
{
public:
	RenderLab(HWND hwnd, int h, int w);
	~RenderLab();

	virtual bool Initialize()override;

	virtual void Update()override;
	virtual void Draw()override;

private:
	virtual void OnResize()override;

	bool InitImGui();
	
	void BuildRootSignaturn();
	void BuildShadersAndIputLayout();
	void BuildGoeMesh();
	void BuildPSOs();

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unique_ptr<MeshGeometry> mGeos;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
};

