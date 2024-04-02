#include "RenderLab.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
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
	BuildMaterial();
	BuildRenderItem();
	BuildFrameresouce();
	BuildPSOs();

	ThrowIfFailed(mCmdList->Close());
	ID3D12CommandList* cmdList[] = { mCmdList.Get() };

	mCmdQueue->ExecuteCommandLists(_countof(cmdList), cmdList);
	FlushCommandQueue();

	return true;
}

void RenderLab::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void RenderLab::Update()
{
	UpdateCamera();

	CurrFrameResourceIndex = (CurrFrameResourceIndex + 1) % NumFrameResource;
	mCurrFrameResource = mFrameResources[CurrFrameResourceIndex].get();

	UpdateObjectCB();
	UpdateMaterialCB();
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
	mCmdList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

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

void RenderLab::BuildRootSignaturn()
{
	CD3DX12_ROOT_PARAMETER slotParameter[3];
	slotParameter[0].InitAsConstantBufferView(0);
	slotParameter[1].InitAsConstantBufferView(1);
	slotParameter[2].InitAsConstantBufferView(2);

	CD3DX12_ROOT_SIGNATURE_DESC rootDesc(
		3,
		slotParameter,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> rootBlob;
	ComPtr<ID3DBlob> errorBlor;

	auto hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&rootBlob, &errorBlor
	);

	ThrowIfFailed(
		mDevice->CreateRootSignature(
			0,
			rootBlob->GetBufferPointer(),
			rootBlob->GetBufferSize(),
			IID_PPV_ARGS(&mRootSignature)
		)
	);
}

void RenderLab::BuildShadersAndIputLayout()
{
	mShaders["vs"] = d3dUtil::CompileShader(L"Shaders/BlinnPhone.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ps"] = d3dUtil::CompileShader(L"Shaders/BlinnPhone.hlsl", nullptr, "PS", "ps_5_1");
	
	/*mShaders["vs"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ps"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_1");*/

	mInputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void RenderLab::BuildGoeMesh()
{
	GeometryGenerator gt;
	GeometryGenerator::MeshData grid = gt.CreateGrid(2.0f, 2.0f, 10, 10);
	GeometryGenerator::MeshData sphere = gt.CreateSphere(1.0f, 40, 40);

	UINT mGridVStrat = 0;
	UINT mSphereVStrat = mGridVStrat + grid.Vertices.size();

	UINT mGridIStrat = 0;
	UINT mSphereIStrat = mGridIStrat + grid.Indices32.size();

	SubmeshGeometry mGrid;
	mGrid.IndexCount = grid.Indices32.size();
	mGrid.StartIndexLocation = mGridVStrat;
	mGrid.BaseVertexLocation = mGridVStrat;

	SubmeshGeometry mSphere;
	mSphere.IndexCount = sphere.Indices32.size();
	mSphere.StartIndexLocation = mSphereIStrat;
	mSphere.BaseVertexLocation = mSphereVStrat;

	UINT j = 0;
	UINT n = grid.Vertices.size() + sphere.Vertices.size();
	std::vector<Vertex> vertices(n);
	for (int i = 0; i < grid.Vertices.size(); i++, j++)
	{
		vertices[j].pos = grid.Vertices[i].Position;
		vertices[j].normal = grid.Vertices[i].Normal;
	}
	for (int i = 0; i < sphere.Vertices.size(); i++, j++)
	{
		vertices[j].pos = sphere.Vertices[i].Position;
		vertices[j].normal = sphere.Vertices[i].Normal;
	}
	std::vector<uint16_t> indices;
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

	UINT vertSize = vertices.size() * sizeof(Vertex);
	UINT indSize = indices.size() * sizeof(uint16_t);
	auto geo = std::make_unique<MeshGeometry>();

	ThrowIfFailed(D3DCreateBlob(vertSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vertSize);
	ThrowIfFailed(D3DCreateBlob(indSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), indSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		mDevice.Get(), mCmdList.Get(), vertices.data(), vertSize, geo->VertexBufferUploader
	);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		mDevice.Get(), mCmdList.Get(), indices.data(), indSize, geo->IndexBufferUploader
	);

	geo->Name = "mGeos";
	geo->VertexBufferByteSize = vertSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = indSize;

	geo->DrawArgs["Grid"] = mGrid;
	geo->DrawArgs["Sphere"] = mSphere;

	mGeos[geo->Name] = std::move(geo);
}

void RenderLab::BuildMaterial()
{
	UINT MatIndex = 0;

	auto Grid = std::make_unique<Material>();
	Grid->Name = "Grid";
	Grid->MatCBIndex = 0;//MatIndex++;
	Grid->NumFramesDirty = NumFrameResource;
	Grid->DiffuseAlbedo = XMFLOAT4(0.6f, 0.4f, 0.2f, 1.0f);
	Grid->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	Grid->Roughness = 0.1f;

	auto Sphere = std::make_unique<Material>();
	Sphere->Name = "Sphere";
	Sphere->MatCBIndex = 1;//MatIndex++;
	Sphere->NumFramesDirty = NumFrameResource;
	Sphere->DiffuseAlbedo = XMFLOAT4(Colors::HotPink);
	Sphere->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	Sphere->Roughness = 0.1f;

	mMats[Grid->Name] = std::move(Grid);
	mMats[Sphere->Name] = std::move(Sphere);
}

void RenderLab::BuildRenderItem()
{
	UINT ObjIndex = 0;

	auto Grid = std::make_unique<RenderItem>();
	Grid->Name = "Grid";
	//Grid->SetTranslation(0.0f, 0.0f, 0.0f);
	Grid->SetScale(5.0f, 5.0f, 5.0f);
	Grid->NumFramesDirty = NumFrameResource;
	Grid->ObjCBIndex = ObjIndex++;
	Grid->Geo = mGeos["mGeos"].get();
	Grid->Mat = mMats["Grid"].get();
	Grid->IndexCount = Grid->Geo->DrawArgs["Grid"].IndexCount;
	Grid->StartIndex = Grid->Geo->DrawArgs["Grid"].StartIndexLocation;
	Grid->StartVertex = Grid->Geo->DrawArgs["Grid"].BaseVertexLocation;
	mAllRitems[Grid->Name] = std::move(Grid);

	auto Sphere = std::make_unique<RenderItem>();
	Sphere->Name = "Sphere";
	//Sphere->SetTranslation(0.0f, 0.0f, 0.0f);
	Sphere->SetScale(2.0f, 2.0f, 2.0f);
	Sphere->NumFramesDirty = NumFrameResource;
	Sphere->ObjCBIndex = ObjIndex++;
	Sphere->Geo = mGeos["mGeos"].get();
	Sphere->Mat = mMats["Sphere"].get();
	Sphere->IndexCount = Sphere->Geo->DrawArgs["Sphere"].IndexCount;
	Sphere->StartIndex = Sphere->Geo->DrawArgs["Sphere"].StartIndexLocation;
	Sphere->StartVertex = Sphere->Geo->DrawArgs["Sphere"].BaseVertexLocation;
	mAllRitems[Sphere->Name] = std::move(Sphere);

	for (auto& a : mAllRitems)
	{
		//mRitem.push_back(a.get());
		mRiNames.push_back(a.first);
		mRitem.push_back(a.second.get());
	}
}

void RenderLab::BuildFrameresouce()
{
	for (int i = 0; i < NumFrameResource; i++)
	{
		mFrameResources.push_back(
			std::make_unique<FrameRecouce>(mDevice.Get(), 1, mAllRitems.size(), mMats.size())
		);
	}

	mMainLight.Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainLight.Strength = { 0.6f, 0.6f, 0.6f };
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
	//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
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

//++++++++++++++++++++++++++Draw Item++++++++++++++++++++++++++
void RenderLab::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	auto objCB = mCurrFrameResource->ObjCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	for (auto a : ritems)
	{
		cmdList->IASetVertexBuffers(0, 1, &a->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&a->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(a->PrimitiveType);

		auto cbAddress = objCB->GetGPUVirtualAddress() + a->ObjCBIndex * objCBByteSize;
		auto mbAddress = matCB->GetGPUVirtualAddress() + a->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, cbAddress);
		cmdList->SetGraphicsRootConstantBufferView(1, mbAddress);

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

	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMMATRIX View = XMMatrixLookAtLH(pos, target, up);

	XMStoreFloat4x4(&mView, View);
}

void RenderLab::UpdatePassCB()
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

	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0] = mMainLight;
	//mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	//mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	//mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	//mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	//mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	//mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto passCB = mCurrFrameResource->PassCB.get();
	passCB->CopyData(0, mMainPassCB);
}

void RenderLab::UpdateObjectCB()
{
	auto objCB = mCurrFrameResource->ObjCB.get();

	for (auto& e : mAllRitems)
	{
		auto a = e.second.get();
		if (a->NumFramesDirty)
		{
			XMMATRIX World = XMLoadFloat4x4(&a->GetWorld());

			ObjectConstants temp;
			XMStoreFloat4x4(&temp.World, XMMatrixTranspose(World));

			objCB->CopyData(a->ObjCBIndex, temp);

			a->NumFramesDirty--;
		}
	}
}

void RenderLab::UpdateMaterialCB()
{
	auto matCB = mCurrFrameResource->MaterialCB.get();

	for (auto& e : mMats)
	{
		auto a = e.second.get();

		if (a->NumFramesDirty)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&a->MatTransform);

			MaterialConstants temp;
			temp.DiffuseAlbedo = a->DiffuseAlbedo;
			temp.FresnelR0 = a->FresnelR0;
			temp.Roughness = a->Roughness;
			XMStoreFloat4x4(&temp.MatTransform, XMMatrixTranspose(matTransform));

			matCB->CopyData(a->MatCBIndex, temp);

			a->NumFramesDirty--;
		}
	}
}

//++++++++++++++++++++++++++Draw ImGui++++++++++++++++++++++++++
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

	ImGuiIO& io = ImGui::GetIO(); (void)io;
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
	//CopyRenderTargetToTexture();

	//ImGui::Begin("Rendered Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	// 获取渲染结果纹理的ID
	//ImTextureID textureID = GetRenderTextureID(); // 假设这是一个函数，用于获取渲染结果纹理的ID

	// 将纹理绘制到ImGui窗口上
	//ImVec2 imageSize(mClientHeight, mClientWidth); // 设置图像大小
	//ImGui::Image((ImTextureID)ShaderResourceView().ptr, imageSize);

	//ImGui::End();

	ImGui::Begin("Object List");

	static int currObjName = 0;
	const char** ObjNames = new const char*[mRiNames.size()];
	for (int i = 0; i < mRiNames.size(); i++)
	{
		ObjNames[i] = mRiNames[i].c_str();
	}
	ImGui::Combo("ObjectList", &currObjName, ObjNames, mRiNames.size(), 3);
	auto currRitem = mAllRitems[mRiNames[currObjName]].get();

	if (ImGui::CollapsingHeader("Transform")) {
		
		static float translation[3];
		static float rotation[3];
		static float scale[3];
		for (int i = 0; i < 3; i++)
		{
			translation[i] = currRitem->translation[i];
			rotation[i] = currRitem->rotation[i];
			scale[i] = currRitem->scale[i];
		}

		// 控制物体位置的滑动条
		if (ImGui::SliderFloat3("Translation", translation, -10.0f, 10.0f))
		{
			currRitem->NumFramesDirty = NumFrameResource;
			currRitem->SetTranslation(translation);
		}
		// 控制物体旋转的旋钮
		if (ImGui::SliderFloat3("Rotation", rotation, -180.0f, 180.0f))
		{
			currRitem->NumFramesDirty = NumFrameResource;
			currRitem->SetRotation(rotation);
		}
		// 控制物体缩放的滑动条
		if (ImGui::SliderFloat3("Scale", scale, 0.1f, 10.0f))
		{
			currRitem->NumFramesDirty = NumFrameResource;
			currRitem->SetScale(scale);
		}

		//static float f = 25.0f;
		//ImGui::Text("SetNextItemWidth/PushItemWidth(100)");
		//ImGui::SameLine(); //HelpMarker("Fixed width.");
		//ImGui::PushItemWidth(100);
		//ImGui::DragFloat("float##1b", &f);
	}

	if (ImGui::CollapsingHeader("Material")) {
		ImVec4 diffuse = {
			currRitem->Mat->DiffuseAlbedo.x,
			currRitem->Mat->DiffuseAlbedo.y,
			currRitem->Mat->DiffuseAlbedo.z,
			currRitem->Mat->DiffuseAlbedo.w,
		};
		static float fresnelR0 = currRitem->Mat->FresnelR0.x;
		static float roughness = currRitem->Mat->Roughness;

		if (ImGui::ColorEdit3("Diffuse", (float*)&diffuse))
		{
			currRitem->Mat->SetDiffuseAlbedo(diffuse.x, diffuse.y, diffuse.z, diffuse.w);
			currRitem->Mat->NumFramesDirty = NumFrameResource;
		}
		
		if (ImGui::SliderFloat("FresnelR0", &fresnelR0, 0.0f, 1.0f))
		{
			currRitem->Mat->SetFresnelR0(fresnelR0);
			currRitem->Mat->NumFramesDirty = NumFrameResource;
		}
		
		if (ImGui::SliderFloat("Roughness", &roughness, 0.005f, 1.0f))
		{
			currRitem->Mat->SetRoughness(roughness);
			currRitem->Mat->NumFramesDirty = NumFrameResource;
		}
	}

	ImGui::End();

	bool show_demo_window = true;
	bool show_another_window = false;
	static float f = 0.0f;
	static int counter = 0;

	//ImGui::ShowDemoWindow(&show_demo_window);

	ImGui::Begin("World");                          // Create a window called "Hello, world!" and append into it.

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

	ImGui::SeparatorText("Main Light");
	ImVec4 lightColor = { mMainLight.Strength.x,mMainLight.Strength.y, mMainLight.Strength.z, 1.0f };
	float lightDir[3] = { mMainLight.Direction.x,mMainLight.Direction.y,mMainLight.Direction.z };
	if (ImGui::ColorEdit3("Light Color", (float*)&lightColor))
	{
		mMainLight.Strength = { lightColor.x, lightColor.y, lightColor.z };
	}
	if (ImGui::SliderFloat3("Lirght Dir", lightDir, -180.0f, 180.0f))
	{
		mMainLight.Direction = { lightDir[0], lightDir[1], lightDir[2] };
	}

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