#pragma once

#include <vector>
#include <wrl.h>

#include <DirectX/d3dx12.h>
#include <dxgi1_6.h>

class GpuResource;

namespace SwapChainBuffer {
	const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	class SwapChainBufferClass {
	public:
		SwapChainBufferClass() = default;
		virtual ~SwapChainBufferClass() = default;

	public:
		__forceinline GpuResource* BackBuffer(UINT index) const;
		__forceinline GpuResource* CurrentBackBuffer() const;
		__forceinline D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferRtv() const;
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE CurrentBackBufferSrv() const;
		__forceinline constexpr UINT CurrentBackBufferIndex() const;

	public:
		BOOL Initialize(ID3D12Device* const device, ID3D12DescriptorHeap* const rtvHeap, UINT count, UINT descSize);
		BOOL LowOnResize(IDXGISwapChain* const swapChain, UINT width, UINT height, BOOL tearing = FALSE);
		BOOL OnResize();

		void NextBackBuffer();

		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			UINT descSize);
		BOOL BuildDescriptors();

	private:

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

#include "SwapChainBuffer.inl"