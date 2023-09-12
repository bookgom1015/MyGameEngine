#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <memory>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;
class GpuResource;

namespace DiffuseSpecularSplitor {
	namespace RootSignatureLayout {
		enum {
			ESI_Diffuse = 0,
			ESI_Specular,
			Count
		};
	}

	static const UINT NumRenderTargets = 3;

	class DiffuseSpecularSplitorClass {
	public:
		DiffuseSpecularSplitorClass();
		virtual ~DiffuseSpecularSplitorClass() = default;

	public:
		__forceinline GpuResource* DiffuseReflectanceMap();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DiffuseReflectanceMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DiffuseReflectanceMapRtv() const;

		__forceinline GpuResource* SpecularReflectanceMap();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SpecularReflectanceMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SpecularReflectanceMapRtv() const;

		__forceinline GpuResource* ReflectivityMap();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ReflectivityMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ReflectivityMapRtv() const;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();

		void Composite(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		bool OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		bool BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		std::unique_ptr<GpuResource> mDiffuseMap;
		std::unique_ptr<GpuResource> mSpecularMap;
		std::unique_ptr<GpuResource> mReflectivityMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDiffuseMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhSpecularMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReflectivityMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhReflectivityMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReflectivityMapCpuRtv;
	};
}

GpuResource* DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::DiffuseReflectanceMap() {
	return mDiffuseMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::DiffuseReflectanceMapSrv() const {
	return mhDiffuseMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::DiffuseReflectanceMapRtv() const {
	return mhDiffuseMapCpuRtv;
}

GpuResource* DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::SpecularReflectanceMap() {
	return mSpecularMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::SpecularReflectanceMapSrv() const {
	return mhSpecularMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::SpecularReflectanceMapRtv() const {
	return mhSpecularMapCpuRtv;
}

GpuResource* DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::ReflectivityMap() {
	return mReflectivityMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::ReflectivityMapSrv() const {
	return mhReflectivityMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DiffuseSpecularSplitor::DiffuseSpecularSplitorClass::ReflectivityMapRtv() const {
	return mhReflectivityMapCpuRtv;
}