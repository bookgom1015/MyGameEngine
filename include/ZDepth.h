#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "GpuResource.h"
#include "HlslCompaction.h"
#include "Light.h"

namespace ZDepth {
	static const UINT NumDepthStenciles = MaxLights;

	class ZDepthClass {
	public:
		ZDepthClass();
		virtual ~ZDepthClass() = default;

	public:
		__forceinline GpuResource* ZDepthMap(UINT index) const;

	public:
		BOOL Initialize(ID3D12Device* const device, UINT texW, UINT texH);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
			UINT descSize, UINT dsvDescSize);

		BOOL AddLight(UINT lightType, UINT index);

	private:
		void BuildDescriptor(UINT index, BOOL needCubemap);

	private:
		ID3D12Device* md3dDevice;

		std::unique_ptr<GpuResource> mZDepthMaps[MaxLights];

		D3D12_CPU_DESCRIPTOR_HANDLE mhCpuDescs[MaxLights];
		D3D12_GPU_DESCRIPTOR_HANDLE mhGpuDescs[MaxLights];

		D3D12_CPU_DESCRIPTOR_HANDLE mhCpuDsvs[MaxLights];

		UINT mTexWidth;
		UINT mTexHeight;
	};
}

GpuResource* ZDepth::ZDepthClass::ZDepthMap(UINT index) const {
	return mZDepthMaps[index].get();
}