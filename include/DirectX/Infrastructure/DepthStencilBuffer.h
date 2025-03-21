#pragma once

#include <DirectX/d3dx12.h>
#include <wrl.h>

#include "DirectX/Infrastructure/GpuResource.h"

namespace DepthStencilBuffer {
	class DepthStencilBufferClass {
	public:
		DepthStencilBufferClass();
		virtual ~DepthStencilBufferClass() = default;

	public:
		__forceinline GpuResource* Resource();

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	public:
		BOOL Initialize(ID3D12Device* const device);
		BOOL LowOnResize(UINT width, UINT height);

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv, UINT dsvDescSize);

	private:
		void BuildDescriptors();
		BOOL BuildResources();

	private:
		ID3D12Device* md3dDevice;

		UINT mWidth;
		UINT mHeight;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

		std::unique_ptr<GpuResource> mDepthStencilBuffer;
	};
}

#include "DepthStencilBuffer.inl"