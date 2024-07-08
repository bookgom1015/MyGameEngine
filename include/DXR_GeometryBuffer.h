#pragma once

#include <d3dx12.h>
#include <array>
#include <wrl.h>

#include "HlslCompaction.h"

struct MeshGeometry;

namespace DXR_GeometryBuffer {
	static const UINT GeometryDescriptorCount = DXR_GeometryBuffer::GeometryBufferCount * 2;

	class DXR_GeometryBufferClass {
	public:
		DXR_GeometryBufferClass() = default;
		virtual ~DXR_GeometryBufferClass() = default;

	public:
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE VerticesSrv() const;
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE IndicesSrv() const;

	public:
		BOOL Initialize(ID3D12Device5* const device);

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);
		void AddGeometry(MeshGeometry* geo);

	private:
		ID3D12Device5* md3dDevice;

		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DXR_GeometryBuffer::GeometryBufferCount> mResources;
		D3D12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
		D3D12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
		UINT mDescSize;

		UINT mNumGeometries;
	};
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE DXR_GeometryBuffer::DXR_GeometryBufferClass::VerticesSrv() const {
	return mhGpuSrv;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXR_GeometryBuffer::DXR_GeometryBufferClass::IndicesSrv() const {
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mhGpuSrv, GeometryBufferCount, mDescSize);
}