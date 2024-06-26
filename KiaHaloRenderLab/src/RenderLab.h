#pragma once

#include "Dx12Render/GeometryGenerator.h"
#include "Dx12Render/MathHelper.h"
#include "Dx12Render/UploadBuffer.h"
#include "Dx12Render/D3DApp.h"
#include "ImGui/KiaHaloUI.h"
#include "RenderObjects.h"
#include <DirectXColors.h>

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

	void UpdateCamera();
	void UpdatePassCB();
	void UpdateMaterialCB();
	void UpdateObjectCB();

	bool InitImGui();
	void RenderImGui();
	void ShutdownImGui();
	
	void BuildRootSignaturn();
	void BuildShadersAndIputLayout();
	void BuildGoeMesh();
	void BuildMaterial();
	void BuildRenderItem();
	void BuildFrameresouce();
	void BuildPSOs();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	ImTextureID GetRenderTextureID();
private:

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mImGuiSrvHeap;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	//std::unique_ptr<MeshGeometry> mGeos;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeos;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::unordered_map<std::string, std::unique_ptr<Material>> mMats;

	//std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::unordered_map<std::string, std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<std::string> mRiNames;
	std::vector<RenderItem*> mRitem;

	const int NumFrameResource = 3;
	std::vector<std::unique_ptr<FrameRecouce>> mFrameResources;
	int CurrFrameResourceIndex = 0;
	FrameRecouce* mCurrFrameResource;
	
	PassConstants mMainPassCB;

	DirectX::XMFLOAT3 mEyePos = { 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	Light mMainLight1;
	Light mMainLight2;

	float mTheta = 1.5f * MathHelper::Pi;
	float mPhi = 0.2f * MathHelper::Pi;
	float mRadius = 15.0f;

	ImVec4 clear_color = ImVec4(1.000000000f, 0.713725507f, 0.756862819f, 1.000000000f); //Colors::LightPink
};