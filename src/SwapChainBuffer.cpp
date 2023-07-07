#include "SwapChainBuffer.h"
#include "Logger.h"

using namespace SwapChainBuffer;

void SwapChainBufferClass::NextBackBuffer() {
	mCurrBackBuffer = (mCurrBackBuffer + 1) % mSwapChainBufferCount;
}

bool SwapChainBufferClass::Initialize(ID3D12Device* pDevice, ID3D12DescriptorHeap* pRtvHeap, UINT count, UINT descSize) {
	md3dDevice = pDevice;

	mSwapChainBuffer.resize(count);
	mSwapChainBufferCount = count;
	mCurrBackBuffer = 0;

	mRtvHeap = pRtvHeap;
	mRtvDescriptorSize = descSize;

	return true;
}

bool SwapChainBufferClass::OnResize(IDXGISwapChain*const pSwapChain, UINT width, UINT height, DXGI_FORMAT format) {
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	// Resize the previous resources we will be creating.
	for (UINT i = 0; i < mSwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();

	// Resize the swap chain.
	CheckHRESULT(pSwapChain->ResizeBuffers(
		mSwapChainBufferCount,
		width,
		height,
		format,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)
	);

	for (UINT i = 0; i < mSwapChainBufferCount; ++i) {
		CheckHRESULT(pSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));

		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	mCurrBackBuffer = 0;

	return true;
}