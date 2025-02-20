#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "GpuResource.h"
#include "HlslCompaction.h"
#include "Light.h"
#include "Locker.h"

namespace ZDepth {
	class ZDepthClass {
	public:
		ZDepthClass();
		virtual ~ZDepthClass() = default;

	public:
		__forceinline GpuResource* ZDepthMap(UINT index) const;
		__forceinline GpuResource* FaceIDCubeMap(UINT index) const;

		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE ZDepthSrv() const;
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE ZDepthCubeSrv() const;
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE FaceIDCubeSrv() const;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, UINT texW, UINT texH);
		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
			UINT descSize, UINT dsvDescSize);

		BOOL AddLight(UINT lightType, UINT index);

	private:
		BOOL BuildDescriptor(ID3D12Device5* const device, UINT index, BOOL needCubemap);

	private:
		Locker<ID3D12Device5>* md3dDevice;

		std::unique_ptr<GpuResource> mZDepthMaps[MaxLights];
		std::unique_ptr<GpuResource> mFaceIDCubeMaps[MaxLights];

		D3D12_CPU_DESCRIPTOR_HANDLE mhZDepthCpuDescs[MaxLights];
		D3D12_GPU_DESCRIPTOR_HANDLE mhZDepthGpuDescs[MaxLights];

		D3D12_CPU_DESCRIPTOR_HANDLE mhZDepthCubeCpuDescs[MaxLights];
		D3D12_GPU_DESCRIPTOR_HANDLE mhZDepthCubeGpuDescs[MaxLights];

		D3D12_CPU_DESCRIPTOR_HANDLE mhFaceIDCubeCpuDescs[MaxLights];
		D3D12_GPU_DESCRIPTOR_HANDLE mhFaceIDCubeGpuDescs[MaxLights];

		UINT mTexWidth;
		UINT mTexHeight;
	};
}

#include "ZDepth.inl"