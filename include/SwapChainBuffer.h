#pragma once

#include <d3dx12.h>
#include <dxgi1_6.h>
#include <vector>
#include <wrl.h>

class GpuResource;

namespace SwapChainBuffer {
	const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	class SwapChainBufferClass {
	public:
		SwapChainBufferClass() = default;
		virtual ~SwapChainBufferClass() = default;

	public:
		__forceinline GpuResource* BackBuffer(int index) const;
		__forceinline GpuResource* CurrentBackBuffer() const;
		__forceinline D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferRtv() const;
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE CurrentBackBufferSrv() const;
		__forceinline constexpr UINT CurrentBackBufferIndex() const;

	public:
		bool Initialize(ID3D12Device* device, ID3D12DescriptorHeap* rtvHeap, UINT count, UINT descSize);
		bool LowOnResize(IDXGISwapChain*const swapChain, UINT width, UINT height);
		bool OnResize();

		void NextBackBuffer();

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv, 
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			UINT descSize);

	private:
		void BuildDescriptors();

	private:
		ID3D12Device* md3dDevice;

		std::vector<std::unique_ptr<GpuResource>> mSwapChainBuffer;
		UINT mSwapChainBufferCount;
		UINT mCurrBackBuffer;

		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mhBackBufferCpuSrvs;
		std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> mhBackBufferGpuSrvs;

		ID3D12DescriptorHeap* mRtvHeap;
		UINT mRtvDescriptorSize;
	};
}

GpuResource* SwapChainBuffer::SwapChainBufferClass::BackBuffer(int index) const {
	return mSwapChainBuffer[index].get();
}

GpuResource* SwapChainBuffer::SwapChainBufferClass::CurrentBackBuffer() const {
	return mSwapChainBuffer[mCurrBackBuffer].get(); 
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferRtv() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferSrv() const {
	return mhBackBufferGpuSrvs[mCurrBackBuffer];
}

constexpr UINT SwapChainBuffer::SwapChainBufferClass::CurrentBackBufferIndex() const {
	return mCurrBackBuffer;
}