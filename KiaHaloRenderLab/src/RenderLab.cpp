#include "RenderLab.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

RenderLab::RenderLab(HWND hwnd, int h, int w)
	: D3DApp(hwnd, h, w)
{
}

RenderLab::~RenderLab()
{
}

bool RenderLab::Initialize()
{
	if (!D3DApp::Initialize())
		return false;
	if (!InitImGui())
		return false;

	ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), nullptr));

	BuildRootSignaturn();
	BuildShadersAndIputLayout();
	BuildGoeMesh();
	BuildPSOs();

	ThrowIfFailed(mCmdList->Close());
	ID3D12CommandList* cmdList[] = {mCmdList.Get()};

	mCmdQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
	FlushCmdQueue();

	return true;
}

void RenderLab::OnResize()
{
	D3DApp::OnResize();
}

void RenderLab::Update()
{

}

void RenderLab::Draw()
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mCmdAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), mPSOs["test"].Get()));

	// Indicate a state transition on the resource usage.
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	mCmdList->RSSetViewports(1, &mScreenViewport);
	mCmdList->RSSetScissorRects(1, &mScissorRect);

	// Clear the back buffer and depth buffer.
	mCmdList->ClearRenderTargetView(CurrBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCmdList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	//mCmdList->OMSetRenderTargets(1, &CurrBackBufferView(), true, &DepthStencilView());
	mCmdList->OMSetRenderTargets(1, &CurrBackBufferView(), FALSE, nullptr);

	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCmdList->IASetVertexBuffers(0, 1, &mGeos->VertexBufferView());

	mCmdList->DrawInstanced(3,1,0,0);
	
	

	// Indicate a state transition on the resource usage.
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCmdQueue();
}

bool RenderLab::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	io.ConfigViewportsNoAutoMerge = true;

	ImGui_ImplWin32_Init(mWnd);
	
	return true;
}

void RenderLab::BuildRootSignaturn()
{
	// 创建一个空的root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void RenderLab::BuildShadersAndIputLayout()
{
	HRESULT hr = S_OK;

	mShaders["vs"] = d3dUtil::CompileShader(L"Shaders\\color.txt", nullptr, "VS", "vs_5_0");
	mShaders["ps"] = d3dUtil::CompileShader(L"Shaders\\color.txt", nullptr, "PS", "ps_5_0");

	mInputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void RenderLab::BuildGoeMesh()
{
	std::vector<Vertex> vertices = {
		{XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(Colors::Red)},
		{XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(Colors::Blue)},
		{XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(Colors::Green)},
	};

	UINT vertSize = (UINT)vertices.size() * sizeof(Vertex);

	mGeos = std::make_unique<MeshGeometry>();
	mGeos->Name = "GeoMesh";

	ThrowIfFailed(D3DCreateBlob(vertSize, &mGeos->VertexBufferCPU));
	CopyMemory(mGeos->VertexBufferCPU->GetBufferPointer(), vertices.data(), vertSize);

	mGeos->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		mDevice.Get(), mCmdList.Get(), vertices.data(), vertSize, mGeos->VertexBufferUploader
	);

	mGeos->VertexBufferByteSize = vertSize;
	mGeos->VertexByteStride = sizeof(Vertex);
}

void RenderLab::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["vs"]->GetBufferPointer()),
		mShaders["vs"]->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ps"]->GetBufferPointer()),
		mShaders["ps"]->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["test"])));
}