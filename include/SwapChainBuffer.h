#pragma once

#include <d3dx12.h>
#include <dxgi1_6.h>
#include <vector>

namespace SwapChainBuffer {
	class SwapChainBufferClass {
	public:
		SwapChainBufferClass() = default;
		virtual ~SwapChainBufferClass() = default;

	public:
		__forceinline ID3D12Resource* BackBuffer(int index) const;
		__forceinline ID3D12Resource* CurrentBackBuffer() const;
		__forceinline D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
		__forceinline constexpr UINT CurrentBackBufferIndex() const;

		void NextBackBuffer();

	public:
		bool Initialize(ID3D12Device* pDevice, ID3D12DescriptorHeap* pRtvHeap, UINT count, UINT descSize);

		bool OnResize(IDXGISwapChain*const pSwapChain, UINT width, UINT height, DXGI_FORMAT format);

	private:
		ID3D12Device* md3dDevice;

		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mSwapChainBuffer;
		UINT mSwapChainBufferCount;
		UINT mCurrBackBuffer;

		ID3D12DescriptorHeap* mRtvHeap;
		UINT mRtvDescriptorSize;
	};
}

ID3D12Resource* SwapChainBuffer::SwapChainBufferClass::BackBuffer(int index) const {
	return mSwapChainBuffer[index].Get();
}

ID3D12Resource* SwapChainBuffer::SwapChainBufferClass::CurrentBackBuffer() const { 
	return mSwapChainBuffer[mCurrBackBuffer].Get(); 
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferView() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

constexpr UINT SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferIndex() const {
	return mCurrBackBuffer;
}