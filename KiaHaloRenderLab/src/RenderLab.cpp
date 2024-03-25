#include "RenderLab.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

RenderLab::RenderLab(HWND hwnd, int h, int w)
	: D3DApp(hwnd, h, w)
{
}

RenderLab::~RenderLab()
{
	if (mDevice != nullptr)
	{
		ShutdownImGui();
		FlushCommandQueue();
	}
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
	BuildRenderItem();
	BuildFrameresouce();
	BuildPSOs();

	ThrowIfFailed(mCmdList->Close());
	ID3D12CommandList* cmdList[] = {mCmdList.Get()};

	mCmdQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
	FlushCommandQueue();

	return true;
}

void RenderLab::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void RenderLab::Update()
{
	UpdateCamera();

	CurrFrameResourceIndex = (CurrFrameResourceIndex + 1) % NumFrameResource;
	mCurrFrameResource = mFrameResources[CurrFrameResourceIndex].get();

	UpdateObjectCB();
	UpdatePassCB();
}

void RenderLab::Draw()
{
	auto cmdListAlloc = mCurrFrameResource->cmdAlloc;
	
	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCmdList->Reset(cmdListAlloc.Get(), mPSOs["test"].Get()));

	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCmdList->RSSetViewports(1, &mScreenViewport);
	mCmdList->RSSetScissorRects(1, &mScissorRect);

	float cl[4] = { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
	mCmdList->ClearRenderTargetView(CurrBackBufferView(), cl, 0, nullptr); //Colors::LightPink
	mCmdList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCmdList->OMSetRenderTargets(1, &CurrBackBufferView(), true, &DepthStencilView());
	//mCmdList->OMSetRenderTargets(1, &CurrBackBufferView(), FALSE, nullptr);
	
	mCmdList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCmdList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	DrawRenderItems(mCmdList.Get(), mRitem);

	RenderImGui();

	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCmdList->Close());

	ID3D12CommandList* cmdsLists[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

bool RenderLab::InitImGui()
{
	D3D12_DESCRIPTOR_HEAP_DESC imguiSrv;
	imguiSrv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	imguiSrv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	imguiSrv.NumDescriptors = 1;
	imguiSrv.NodeMask = 0;
	ThrowIfFailed(
		mDevice->CreateDescriptorHeap(
			&imguiSrv,
			IID_PPV_ARGS(&mImGuiSrvHeap)
		)
	);

	ImGuiIO& io = ImGui::GetIO();(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	io.ConfigViewportsNoAutoMerge = true;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplWin32_Init(mWnd);
	ImGui_ImplDX12_Init(
		mDevice.Get(),
		SwapChainBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		mImGuiSrvHeap.Get(),
		mImGuiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
		mImGuiSrvHeap->GetGPUDescriptorHandleForHeapStart()
	);


	ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	
	return true;
}

void RenderLab::RenderImGui()
{
	ImGuiIO& io = ImGui::GetIO();
	// 开始Dear ImGui的新帧
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 在这里绘制你的ImGui窗口和UI元素
	/*ImGui::Begin("My Window");
	ImGui::Text("Hello, world!");
	ImGui::End();
	*/

	ImGui::Begin("Object List");

	if (ImGui::CollapsingHeader("Transform")) {
		// 控制物体位置的滑动条
		static float translation[3] = { 0.0f, 0.0f, 0.0f };
		ImGui::SliderFloat3("Translation", translation, -10.0f, 10.0f);

		// 控制物体旋转的旋钮
		static float rotation[3] = { 0.0f, 0.0f, 0.0f };
		ImGui::SliderFloat3("Rotation", rotation, -180.0f, 180.0f);

		// 控制物体缩放的滑动条
		static float scale = 2.0f;
		ImGui::SliderFloat("Scale", &scale, 0.1f, 10.0f);

		for (auto& a : mAllRitems)
		{
			a->NumFramesDirty = NumFrameResource;
			XMStoreFloat4x4(&a->World, XMMatrixScaling(scale,scale,scale));
		}
	}

	ImGui::End();

	bool show_demo_window = true;
	bool show_another_window = false;
	static float f = 0.0f;
	static int counter = 0;

	ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

	ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
	ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
	ImGui::Checkbox("Another Window", &show_another_window);

	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
	ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

	if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
		counter++;
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);

	ImGui::Text("Application average %.1f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
	ImGui::End();

	// 渲染Dear ImGui
	ImGui::Render();


	mCmdList->SetDescriptorHeaps(1, mImGuiSrvHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCmdList.Get());
}

void RenderLab::ShutdownImGui()
{
	// 关闭ImGui的渲染器后端
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();

	// 销毁Dear ImGui上下文
	ImGui::DestroyContext();
}

void RenderLab::BuildRootSignaturn()
{
	CD3DX12_ROOT_PARAMETER slotRootParamater[2];
	slotRootParamater[0].InitAsConstantBufferView(0);
	slotRootParamater[1].InitAsConstantBufferView(1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		2,
		slotRootParamater,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> rootSig;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		rootSig.GetAddressOf(), errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(
		mDevice->CreateRootSignature(
			0,
			rootSig->GetBufferPointer(),
			rootSig->GetBufferSize(),
			IID_PPV_ARGS(&mRootSignature)
		)
	);
}

void RenderLab::BuildShadersAndIputLayout()
{
	mShaders["vs"] = d3dUtil::CompileShader(L"Shaders\\default.txt", nullptr, "VS", "vs_5_0");
	mShaders["ps"] = d3dUtil::CompileShader(L"Shaders\\default.txt", nullptr, "PS", "ps_5_0");

	mInputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void RenderLab::BuildGoeMesh()
{
	GeometryGenerator gt;
	GeometryGenerator::MeshData mSphere = gt.CreateSphere(1.0f, 20, 20);

	SubmeshGeometry Sphere;
	Sphere.IndexCount = mSphere.Indices32.size();
	Sphere.BaseVertexLocation = 0;
	Sphere.StartIndexLocation = 0;

	UINT n = 0 + mSphere.Vertices.size();
	std::vector<Vertex> vertices(n);
	for (int i = 0; i < mSphere.Vertices.size(); i++)
	{
		vertices[i].pos = mSphere.Vertices[i].Position;
		vertices[i].color = (XMFLOAT4)Colors::AliceBlue;
	}
	std::vector<uint16_t> indices;
	indices.insert(indices.end(), mSphere.GetIndices16().begin(), mSphere.GetIndices16().end());

	UINT vertSize = (UINT)vertices.size() * sizeof(Vertex);
	UINT indSize = (UINT)indices.size() * sizeof(uint16_t);

	mGeos = std::make_unique<MeshGeometry>();
	mGeos->Name = "GeoMesh";

	ThrowIfFailed(D3DCreateBlob(vertSize, &mGeos->VertexBufferCPU));
	CopyMemory(mGeos->VertexBufferCPU->GetBufferPointer(), vertices.data(), vertSize);

	ThrowIfFailed(D3DCreateBlob(indSize, &mGeos->IndexBufferCPU));
	CopyMemory(mGeos->IndexBufferCPU->GetBufferPointer(), indices.data(), indSize);

	mGeos->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		mDevice.Get(), mCmdList.Get(), vertices.data(), vertSize, mGeos->VertexBufferUploader
	);
	mGeos->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		mDevice.Get(), mCmdList.Get(), indices.data(), indSize, mGeos->IndexBufferUploader
	);

	mGeos->VertexBufferByteSize = vertSize;
	mGeos->VertexByteStride = sizeof(Vertex);
	mGeos->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeos->IndexBufferByteSize = indSize;

	mGeos->DrawArgs["sphere"] = Sphere;
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
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
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

void RenderLab::BuildRenderItem()
{
	UINT objIndex = 0;
	auto sphere = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sphere->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	sphere->NumFramesDirty = NumFrameResource;
	sphere->ObjCBIndex = 0;
	sphere->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphere->Geo = mGeos.get();
	sphere->IndexCount = sphere->Geo->DrawArgs["sphere"].IndexCount;
	sphere->StartIndex = sphere->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphere->StartVertex = sphere->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRitems.push_back(std::move(sphere));

	for (auto& a : mAllRitems)
		mRitem.push_back(a.get());
}

void RenderLab::BuildFrameresouce()
{
	for (int i = 0; i < NumFrameResource; i++)
	{
		mFrameResources.push_back(
			std::make_unique<FrameRecouce>(mDevice.Get(), 1, mAllRitems.size(), 0)
		);
	}
}

//++++++++++++++++++++++++++Draw++++++++++++++++++++++++++
void RenderLab::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	auto objCB = mCurrFrameResource->ObjCB->Resource();

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for (auto a : ritems)
	{
		cmdList->IASetVertexBuffers(0, 1, &a->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&a->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(a->PrimitiveType);

		auto cbAddress = objCB->GetGPUVirtualAddress() + a->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, cbAddress);

		cmdList->DrawIndexedInstanced(
			a->IndexCount,
			1,
			a->StartIndex,
			a->StartVertex,
			0
		);
	}
}

//++++++++++++++++++++++++++Update Render Object++++++++++++++++++++++++++
void RenderLab::UpdateCamera()
{
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	XMVECTOR pos = { mEyePos.x, mEyePos.y, mEyePos.z, 1.0f };
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);

	XMStoreFloat4x4(&mView, view);
}

void RenderLab::UpdatePassCB()
{
	auto objCB = mCurrFrameResource->ObjCB.get();

	for (auto& a : mAllRitems)
	{
		if (a->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&a->World);

			ObjectConstants temp;
			XMStoreFloat4x4(&temp.World, world);

			objCB->CopyData(a->ObjCBIndex, temp);

			a->NumFramesDirty--;
		}
	}
}

void RenderLab::UpdateObjectCB()
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

	mMainPassCB.EyePosW = mEyePos;

	auto passCB = mCurrFrameResource->PassCB.get();
	passCB->CopyData(0, mMainPassCB);
}