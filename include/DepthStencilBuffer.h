#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "GpuResource.h"

namespace DepthStencilBuffer {
	const DXGI_FORMAT Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	class DepthStencilBufferClass {
	public:
		DepthStencilBufferClass();
		virtual ~DepthStencilBufferClass() = default;

	public:
		__forceinline GpuResource* Resource();

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	public:
		bool Initialize(ID3D12Device* device);
		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv, UINT dsvDescSize);

		bool OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		bool BuildResources();

	private:
		ID3D12Device* md3dDevice;

		UINT mWidth;
		UINT mHeight;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

		std::unique_ptr<GpuResource> mDepthStencilBuffer;
	};
}

GpuResource* DepthStencilBuffer::DepthStencilBufferClass::Resource() {
	return mDepthStencilBuffer.get();
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthStencilBuffer::DepthStencilBufferClass::Dsv() const {
	return mhCpuDsv;
}