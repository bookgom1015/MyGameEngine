#include "DxLowRenderer.h"
#include "Logger.h"

namespace {
	void D3D12MessageCallback(
			D3D12_MESSAGE_CATEGORY category,
			D3D12_MESSAGE_SEVERITY severity,
			D3D12_MESSAGE_ID id,
			LPCSTR pDescription,
			void *pContext) {
		std::string str(pDescription);

		std::string sevStr;
		switch (severity) {
		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
			sevStr = "Corruption";
			break;
		case D3D12_MESSAGE_SEVERITY_ERROR:
			sevStr = "Error";
			break;
		case D3D12_MESSAGE_SEVERITY_WARNING:
			sevStr = "Warning";
			break;
		case D3D12_MESSAGE_SEVERITY_INFO:
			return;
			sevStr = "Info";
			break;
		case D3D12_MESSAGE_SEVERITY_MESSAGE:
			return;
			sevStr = "Message";
			break;
		}

		std::stringstream sstream;
		sstream << '[' << sevStr << "] " << pDescription;

		Logln(sstream.str());
	}
}

DxLowRenderer::DxLowRenderer() {
	bIsCleanedUp = false;

	mhMainWnd = NULL;

	mCallbakCookie = 0x01010101;

	mdxgiFactoryFlags = 0;

	mRtvDescriptorSize = 0;
	mDsvDescriptorSize = 0;
	mCbvSrvUavDescriptorSize = 0;

	mCurrentFence = 0;

	mRefreshRate = 60;

	mSwapChainBuffer = std::make_unique<SwapChainBuffer::SwapChainBufferClass>();
	mDepthStencilBuffer = std::make_unique<DepthStencilBuffer::DepthStencilBufferClass>();
}

DxLowRenderer::~DxLowRenderer() {
	if (!bIsCleanedUp) LowCleanUp();
}

bool DxLowRenderer::LowInitialize(HWND hwnd, UINT width, UINT height) {
	mhMainWnd = hwnd;

	CheckReturn(InitDirect3D(width, height));

	const auto device = md3dDevice.Get();
	CheckReturn(mSwapChainBuffer->Initialize(device, mRtvHeap.Get(), SwapChainBufferCount, mRtvDescriptorSize));
	CheckReturn(mDepthStencilBuffer->Initialize(device));

	BuildDescriptors();

	CheckReturn(LowOnResize(width, height));

	return true;
}

void DxLowRenderer::LowCleanUp() {
	if (mInfoQueue != nullptr) {
		if (FAILED(mInfoQueue->UnregisterMessageCallback(mCallbakCookie))) {
			WLogln(L"Failed to unregister message call-back");
		}
	}

	if (md3dDevice != nullptr && mCommandQueue != nullptr) {
		if (!FlushCommandQueue())
			WLogln(L"Failed to flush command queue during cleaning up");
	}

	bIsCleanedUp = true;
}

bool DxLowRenderer::LowOnResize(UINT width, UINT height) {
	// Flush before changing any resources.
	CheckReturn(FlushCommandQueue());

	CheckHRESULT(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	
	CheckReturn(mSwapChainBuffer->OnResize(mSwapChain.Get(), width, height, BackBufferFormat));
	CheckReturn(mDepthStencilBuffer->OnResize(width, height));

	// Execute the resize commands.
	CheckHRESULT(mCommandList->Close());

	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait until resize is complete.
	CheckReturn(FlushCommandQueue());

	// Update the viewport
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(width);
	mScreenViewport.Height = static_cast<float>(height);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

	return true;
}

bool DxLowRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 32;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckHRESULT(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mCbvSrvUavHeap)));

	return true;
}

UINT64 DxLowRenderer::IncreaseFence() { return ++mCurrentFence; }

D3D12_CPU_DESCRIPTOR_HANDLE DxLowRenderer::DepthStencilView() const { return mDsvHeap->GetCPUDescriptorHandleForHeapStart(); }

HRESULT DxLowRenderer::GetDeviceRemovedReason() const { return md3dDevice->GetDeviceRemovedReason(); }

bool DxLowRenderer::FlushCommandQueue() {
	// Advance the fence value to mark commands up to this fence point.
	++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point.
	// Because we are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	CheckHRESULT(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has compledted commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.
		CheckHRESULT(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	return true;
}

bool DxLowRenderer::InitDirect3D(UINT width, UINT height) {
#ifdef _DEBUG
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mDebugController)))) {
		mDebugController->EnableDebugLayer();
		mdxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
	}
#endif
	
	CheckHRESULT(CreateDXGIFactory2(mdxgiFactoryFlags, IID_PPV_ARGS(&mdxgiFactory)));

	Adapters adapters;
	SortAdapters(adapters);

	bool found = false;
	for (auto begin = adapters.rbegin(), end = adapters.rend(); begin != end; ++begin) {
		auto adapter = begin->second;

		// Try to create hardware device.
		HRESULT hr = D3D12CreateDevice(
			adapter,
			D3D_FEATURE_LEVEL_12_1,
			__uuidof(ID3D12Device5),
			static_cast<void**>(&md3dDevice)
		);

		if (SUCCEEDED(hr)) {
			found = true;
#ifdef _DEBUG
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);
			WLogln(desc.Description, L" is selected \n");
#endif
			break;
		}
	}
	if (!found) ReturnFalse(L"There are no adapters that support the required features");

	// Checks that device supports ray-tracing.
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 ops = {};
	HRESULT featureSupport = md3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &ops, sizeof(ops));
	if (FAILED(featureSupport)) ReturnFalse(L"Device or driver does not support d3d12");

	if (FAILED(featureSupport) || ops.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
		ReturnFalse(L"Device or driver does not support ray-tracing");

	CheckHRESULT(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#if _DEBUG
	CheckReturn(CreateDebugObjects());
#endif
	CheckReturn(CreateCommandObjects());
	CheckReturn(CreateSwapChain(width, height));
	CheckReturn(CreateRtvAndDsvDescriptorHeaps());

	return true;
}

void DxLowRenderer::SortAdapters(Adapters& adapters) {
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;

#if _DEBUG
	WLogln(L"Adapters:");
	std::wstringstream wsstream;
#endif

	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

#if _DEBUG
		wsstream << L"\t " << desc.Description << std::endl;
#endif

		size_t score = desc.DedicatedSystemMemory + desc.DedicatedVideoMemory + desc.SharedSystemMemory;

		adapters.insert(std::make_pair(score, adapter));
		++i;
	}

#if _DEBUG
	WLog(wsstream.str());
#endif
}

bool DxLowRenderer::CreateDebugObjects() {
	CheckHRESULT(md3dDevice->QueryInterface(IID_PPV_ARGS(&mInfoQueue)));

	CheckHRESULT(mInfoQueue->RegisterMessageCallback(D3D12MessageCallback, D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS, NULL, &mCallbakCookie));

	return true;
}

bool DxLowRenderer::CreateCommandObjects() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	CheckHRESULT(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf()))
	);
	
	CheckHRESULT(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(),	// Associated command allocator
		nullptr,					// Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())
	));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();

	return true;
}

bool DxLowRenderer::CreateSwapChain(UINT width, UINT height) {
	// Release the previous swapchain we will be recreating.
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.RefreshRate.Numerator = mRefreshRate;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perfrom flush.
	CheckHRESULT(mdxgiFactory->CreateSwapChain(mCommandQueue.Get(), &sd, mSwapChain.GetAddressOf()));

	return true;
}

void DxLowRenderer::BuildDescriptors() {
	auto cpuStart = mCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
	auto gpuStart = mCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
	auto rtvCpuStart = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

	mhCpuCbvSrvUav = CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart);
	mhGpuCbvSrvUav = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart);
	mhCpuDsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart);
	mhCpuRtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart);

	mDepthStencilBuffer->BuildDescriptors(mhCpuDsv, mDsvDescriptorSize);
	mhCpuRtv.Offset(SwapChainBufferCount, mRtvDescriptorSize);
}