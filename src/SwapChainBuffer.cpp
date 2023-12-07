#include "SwapChainBuffer.h"
#include "Logger.h"
#include "GpuResource.h"

using namespace SwapChainBuffer;

BOOL SwapChainBufferClass::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* rtvHeap, UINT count, UINT descSize) {
	md3dDevice = device;

	mSwapChainBuffer.resize(count);
	mSwapChainBufferCount = count;
	mCurrBackBuffer = 0;

	mhBackBufferCpuSrvs.resize(count);
	mhBackBufferGpuSrvs.resize(count);

	mRtvHeap = rtvHeap;
	mRtvDescriptorSize = descSize;

	for (UINT i = 0; i < count; ++i) 
		mSwapChainBuffer[i] = std::make_unique<GpuResource>();

	return true;
}

BOOL SwapChainBufferClass::LowOnResize(IDXGISwapChain*const swapChain, UINT width, UINT height, BOOL tearing) {
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

	// Resize the previous resources we will be creating.
	for (UINT i = 0; i < mSwapChainBufferCount; ++i)
		mSwapChainBuffer[i]->Reset();

	// Resize the swap chain.
	CheckHRESULT(swapChain->ResizeBuffers(
		mSwapChainBufferCount,
		width,
		height,
		BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | 
		(tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0)
	));

	for (UINT i = 0; i < mSwapChainBufferCount; ++i) {
		auto buffer = mSwapChainBuffer[i].get();
		buffer->OnResize(swapChain, i);

		md3dDevice->CreateRenderTargetView(buffer->Resource(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	mCurrBackBuffer = 0;

	return true;
}

BOOL SwapChainBufferClass::OnResize() {
	BuildDescriptors();

	return true;
}

void SwapChainBufferClass::NextBackBuffer() {
	mCurrBackBuffer = (mCurrBackBuffer + 1) % mSwapChainBufferCount;
}

void SwapChainBufferClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		UINT descSize) {
	for (UINT i = 0; i < mSwapChainBufferCount; ++i) {
		mhBackBufferCpuSrvs[i] = hCpuSrv;
		mhBackBufferGpuSrvs[i] = hGpuSrv;

		hCpuSrv.Offset(1, descSize);
		hGpuSrv.Offset(1, descSize);
	}

	BuildDescriptors();
}

void SwapChainBufferClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = BackBufferFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	for (UINT i = 0; i < mSwapChainBufferCount; ++i) {
		md3dDevice->CreateShaderResourceView(mSwapChainBuffer[i]->Resource(), &srvDesc, mhBackBufferCpuSrvs[i]);
	}
}