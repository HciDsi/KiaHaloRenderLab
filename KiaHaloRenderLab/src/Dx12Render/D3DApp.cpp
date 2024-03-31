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
		FlushCommandQueue();
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

	CreateDescriptorHeap();

	OnResize();
}

void D3DApp::OnResize()
{
	assert(mDevice);
	assert(mSwapChain);
	assert(mCmdAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

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
	CreateSrv();
	
	mCmdList->Close();
	ID3D12CommandList* cmdList[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(1, cmdList);

	SetViewportAndRedct();
}

//--------------------DirectX 初始化--------------------
void D3DApp::CreateDevice()
{
	// 创建DXGI工厂
	ThrowIfFailed(
		CreateDXGIFactory1(
			IID_PPV_ARGS(&mFactory)
		)
	);

	// 创建Direct3D 12设备
	ThrowIfFailed(
		D3D12CreateDevice(
			nullptr,
			D3D_FEATURE_LEVEL_12_0, // 使用的最低功能级别为Direct3D 12.0
			IID_PPV_ARGS(&mDevice)
		)
	);

	// 获取渲染目标视图描述符大小
	mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 获取深度/模板视图描述符大小
	mDsvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// 获取常量缓冲区/着色器资源/无序访问视图描述符大小
	mCbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3DApp::CreateFence()
{
	// 创建一个新的围栏对象
	ThrowIfFailed(
		mDevice->CreateFence(
			0,                          // 初始围栏值，通常为0
			D3D12_FENCE_FLAG_NONE,     // 围栏标志，这里没有使用任何标志
			IID_PPV_ARGS(&mFence)
		)
	);
}

void D3DApp::CreateCmdObjects()
{
	// 创建命令队列
	D3D12_COMMAND_QUEUE_DESC qd = {}; // 初始化命令队列描述符结构体
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 指定命令队列类型为直接命令列表
	qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // 指定命令队列标志为无
	ThrowIfFailed(
		mDevice->CreateCommandQueue(
			&qd,
			IID_PPV_ARGS(&mCmdQueue) // 创建命令队列并获取其接口指针
		)
	);

	// 创建命令分配器
	ThrowIfFailed(
		mDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, // 指定命令分配器关联的命令列表类型为直接命令列表
			IID_PPV_ARGS(&mCmdAlloc) // 创建命令分配器并获取其接口指针
		)
	);

	// 创建命令列表
	ThrowIfFailed(
		mDevice->CreateCommandList(
			0, // 指定节点掩码为0，表示命令列表在所有GPU节点上都可执行
			D3D12_COMMAND_LIST_TYPE_DIRECT, // 指定命令列表类型为直接命令列表
			mCmdAlloc.Get(), // 关联命令分配器
			nullptr, // 关联流水线状态对象（Pipeline State Object，PSO），这里为空
			IID_PPV_ARGS(&mCmdList) // 创建命令列表并获取其接口指针
		)
	);

	// 关闭命令列表（初始化状态）
	mCmdList->Close();
}

void D3DApp::CreateSwapChain()
{
	// 定义交换链描述符并设置其属性
	DXGI_SWAP_CHAIN_DESC sc;
	sc.BufferDesc.Width = mClientWidth;										// 缓冲区宽度
	sc.BufferDesc.Height = mClientHeight;									// 缓冲区高度
	sc.BufferDesc.RefreshRate.Denominator = 1;								// 刷新率的分母
	sc.BufferDesc.RefreshRate.Numerator = 60;								// 刷新率的分子
	sc.BufferDesc.Format = mBackBufferFormat;								// 缓冲区格式
	sc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;	// 扫描线排序方式
	sc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;					// 缩放方式
	sc.SampleDesc.Count = 1;												// 多重采样的样本数量
	sc.SampleDesc.Quality = 0;												// 多重采样的质量级别
	sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;						// 缓冲区的使用方式
	sc.BufferCount = SwapChainBufferCount;									// 缓冲区数量
	sc.OutputWindow = mWnd;													// 输出窗口句柄
	sc.Windowed = true;														// 是否为窗口模式
	sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;							// 交换链效果
	sc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;						// 交换链标志

	// 创建交换链
	ThrowIfFailed(
		mFactory->CreateSwapChain(
			mCmdQueue.Get(),       // 与交换链相关联的命令队列
			&sc,                   // 交换链描述符
			&mSwapChain            // 用于接收交换链接口指针的变量
		)
	);
}

void D3DApp::CreateDescriptorHeap()
{
	// 创建渲染目标视图（RTV）描述符堆
	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;         // 描述符堆类型为渲染目标视图
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;      // 无标志
	rtvDesc.NumDescriptors = SwapChainBufferCount;         // 描述符数量为交换链缓冲区数量
	rtvDesc.NodeMask = 0;
	ThrowIfFailed(
		mDevice->CreateDescriptorHeap(
			&rtvDesc,                                       // 描述符堆描述符
			IID_PPV_ARGS(&mRtvHeap)                         // 创建渲染目标视图描述符堆
		)
	);

	// 创建深度/模板缓冲区视图（DSV）描述符堆
	D3D12_DESCRIPTOR_HEAP_DESC dsvDesc;
	dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;         // 描述符堆类型为深度/模板缓冲区视图
	dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;       // 无标志
	dsvDesc.NumDescriptors = 1;                            // 描述符数量为1
	dsvDesc.NodeMask = 0;
	ThrowIfFailed(
		mDevice->CreateDescriptorHeap(
			&dsvDesc,                                       // 描述符堆描述符
			IID_PPV_ARGS(&mDsvHeap)                         // 创建深度/模板缓冲区视图描述符堆
		)
	);

	// 创建常量缓冲区、着色器资源视图和无序访问视图（CBV/SRV/UAV）描述符堆
	D3D12_DESCRIPTOR_HEAP_DESC srvDesc;
	srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; 
	srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;       
	srvDesc.NumDescriptors = 1;                            
	srvDesc.NodeMask = 0;
	ThrowIfFailed(
		mDevice->CreateDescriptorHeap(
			&srvDesc,                                       // 描述符堆描述符
			IID_PPV_ARGS(&mSrvHeap)                         // 创建CBV/SRV/UAV描述符堆
		)
	);
}

void D3DApp::CreateRtv()
{
	// 获取渲染目标视图描述符堆的CPU句柄
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	// 遍历交换链缓冲区，为每个缓冲区创建渲染目标视图
	for (int i = 0; i < SwapChainBufferCount; i++)
	{
		// 获取交换链缓冲区
		ThrowIfFailed(
			mSwapChain->GetBuffer(
				i,
				IID_PPV_ARGS(&mSwapChainBuffer[i])
			)
		);

		// 创建渲染目标视图并将其绑定到描述符堆中的相应位置
		mDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get(),  // 缓冲区指针
			nullptr,                     // 渲染目标视图的描述符（nullptr表示使用默认参数）
			rtvHandle                    // 渲染目标视图描述符的CPU句柄
		);

		// 偏移到下一个渲染目标视图描述符
		rtvHandle.Offset(1, mRtvDescriptorSize);
	}
}

void D3DApp::CreateDsv()
{
	D3D12_RESOURCE_DESC rd;
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = mClientWidth;
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

void D3DApp::CreateSrv()
{
	D3D12_RESOURCE_DESC rd;
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = mClientWidth;
	rd.Height = mClientHeight;
	rd.DepthOrArraySize = 1;
	rd.Alignment = 0;
	rd.MipLevels = 1;
	rd.Format = mBackBufferFormat;
	rd.SampleDesc.Count = 1;
	rd.SampleDesc.Quality = 0;
	rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

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
			nullptr,
			IID_PPV_ARGS(&mShaderResouceBuffer)
		)
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = mBackBufferFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	mDevice->CreateShaderResourceView(
		mShaderResouceBuffer.Get(),
		&srvDesc,
		mSrvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	/*mCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mShaderResouceBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		)
	);*/
}

void D3DApp::SetViewportAndRedct()
{
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY =0;
	mScreenViewport.Width = mClientWidth;
	mScreenViewport.Height = mClientHeight;
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;
	
	mScissorRect.top = 0;                      
	mScissorRect.left = 0;                     
	mScissorRect.right = mClientWidth;
	mScissorRect.bottom = mClientHeight; 
}

void D3DApp::FlushCommandQueue()
{
	// 递增当前围栏值
	mCurrentFence++;

	// 向命令队列发出信号，表示当前围栏已经达到指定的值
	ThrowIfFailed(
		mCmdQueue->Signal(mFence.Get(), mCurrentFence)
	);

	// 检查是否已经完成了之前的命令
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		// 创建一个新的事件对象
		auto evenHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		// 设置当围栏值达到指定值时触发事件
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, evenHandle));

		// 等待事件触发，表示当前围栏已经完成
		WaitForSingleObject(evenHandle, INFINITE);

		// 关闭事件句柄
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

D3D12_GPU_DESCRIPTOR_HANDLE D3DApp::ShaderResourceView() const
{
	return mSrvHeap->GetGPUDescriptorHandleForHeapStart();
}

void D3DApp::CopyRenderTargetToTexture()
{
	D3D12_RESOURCE_BARRIER barr = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	);
	mCmdList->ResourceBarrier(1, &barr);

	barr = CD3DX12_RESOURCE_BARRIER::Transition(
		mShaderResouceBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COPY_DEST
	);
	mCmdList->ResourceBarrier(1, &barr);

	mCmdList->CopyResource(mShaderResouceBuffer.Get(), CurrBackBuffer());

	barr = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_COMMON
	);
	mCmdList->ResourceBarrier(1, &barr);

	barr = CD3DX12_RESOURCE_BARRIER::Transition(
		mShaderResouceBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_PRESENT
	);
	mCmdList->ResourceBarrier(1, &barr);
}