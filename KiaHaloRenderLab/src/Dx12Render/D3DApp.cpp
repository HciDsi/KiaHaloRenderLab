#include "D3DApp.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

D3DApp::D3DApp(HWND hwnd, int h, int w)
	: mWnd(hwnd), mClientHeight(h), mClientWidth(w)
{
	// Only one D3DApp can be constructed.
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp::~D3DApp()
{
	if (mDevice != nullptr)
		FlushCmdQueue();
}

D3DApp* D3DApp::mApp = nullptr;

D3DApp* D3DApp::GetApp()
{
	return mApp;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

bool D3DApp::Initialize()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	CreateDevice();
	CreateFence();
	CreateCmdObjects();
	CreateSwapChain();

	CreateRtvAndDsvDescriptorHeap();

	OnResize();
}

void D3DApp::OnResize()
{
	assert(mDevice);
	assert(mSwapChain);
	assert(mCmdAlloc);

	// Flush before changing any resources.
	FlushCmdQueue();

	ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;
	CreateRtv();
	CreateDsv();
	
	mCmdList->Close();
	ID3D12CommandList* cmdList[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(1, cmdList);

	SetViewportAndRedct();
}

//--------------------DirectX 初始化--------------------
void D3DApp::CreateDevice()
{
	ThrowIfFailed(
		CreateDXGIFactory1(
			IID_PPV_ARGS(&mFactory)
		)
	);

	ThrowIfFailed(
		D3D12CreateDevice(
			nullptr,
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&mDevice)
		)
	);

	mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3DApp::CreateFence()
{
	ThrowIfFailed(
		mDevice->CreateFence(
			0,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&mFence)
		)
	);
}

void D3DApp::CreateCmdObjects()
{
	D3D12_COMMAND_QUEUE_DESC qd = {};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(
		mDevice->CreateCommandQueue(
			&qd,
			IID_PPV_ARGS(&mCmdQueue)
		)
	);

	ThrowIfFailed(
		mDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&mCmdAlloc)
		)
	);

	ThrowIfFailed(
		mDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			mCmdAlloc.Get(),
			nullptr,
			IID_PPV_ARGS(&mCmdList)
		)
	);

	mCmdList->Close();
}

void D3DApp::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC sc;
	sc.BufferDesc.Width = mClientWidth;
	sc.BufferDesc.Height = mClientHeight;
	sc.BufferDesc.RefreshRate.Denominator = 1;
	sc.BufferDesc.RefreshRate.Numerator = 60;
	sc.BufferDesc.Format = mBackBufferFormat;
	sc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sc.SampleDesc.Count = 1;
	sc.SampleDesc.Quality = 0;
	sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sc.BufferCount = SwapChainBufferCount;
	sc.OutputWindow = mWnd;
	sc.Windowed = true;
	sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(
		mFactory->CreateSwapChain(
			mCmdQueue.Get(),
			&sc,
			&mSwapChain
		)
	);
}

void D3DApp::CreateRtvAndDsvDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDesc.NumDescriptors = SwapChainBufferCount;
	rtvDesc.NodeMask = 0;
	ThrowIfFailed(
		mDevice->CreateDescriptorHeap(
			&rtvDesc,
			IID_PPV_ARGS(&mRtvHeap)
		)
	);

	D3D12_DESCRIPTOR_HEAP_DESC dsvDesc;
	dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDesc.NumDescriptors = 1;
	dsvDesc.NodeMask = 0;
	ThrowIfFailed(
		mDevice->CreateDescriptorHeap(
			&dsvDesc,
			IID_PPV_ARGS(&mDsvHeap)
		)
	);
}

void D3DApp::CreateRtv()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	for (int i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(
			mSwapChain->GetBuffer(
				i,
				IID_PPV_ARGS(&mSwapChainBuffer[i])
			)
		);

		mDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get(),
			nullptr,
			rtvHandle
		);

		rtvHandle.Offset(1, mRtvDescriptorSize);
	}
}

void D3DApp::CreateDsv()
{
	D3D12_RESOURCE_DESC rd;
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = mClientHeight;
	rd.Height = mClientHeight;
	rd.DepthOrArraySize = 1;
	rd.Alignment = 0;
	rd.MipLevels = 1;
	rd.Format = mDepthStencilFormat;
	rd.SampleDesc.Count = 1;
	rd.SampleDesc.Quality = 0;
	rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE cv;
	cv.Format = mDepthStencilFormat;
	cv.DepthStencil.Depth = 1.0f;
	cv.DepthStencil.Stencil = 0.0f;

	ThrowIfFailed(
		mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rd,
			D3D12_RESOURCE_STATE_COMMON,
			&cv,
			IID_PPV_ARGS(&mDepthStencilBuffer)
		)
	);

	mDevice->CreateDepthStencilView(
		mDepthStencilBuffer.Get(),
		nullptr,
		DepthStencilView()
	);

	mCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		)
	);
}

void D3DApp::SetViewportAndRedct()
{
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = mClientWidth;
	mScreenViewport.Height = mClientHeight;
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;
	
	mScissorRect.top = 0;                      
	mScissorRect.left = 0;                     
	mScissorRect.right = mClientWidth;    
	mScissorRect.bottom = mClientHeight; 
}

void D3DApp::FlushCmdQueue()
{
	mCurrentFence++;
	ThrowIfFailed(
		mCmdQueue->Signal(mFence.Get(), mCurrentFence)
	);

	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		auto evenHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, evenHandle));

		WaitForSingleObject(evenHandle, INFINITE);
		CloseHandle(evenHandle);
	}
}

ID3D12Resource* D3DApp::CurrBackBuffer() const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
