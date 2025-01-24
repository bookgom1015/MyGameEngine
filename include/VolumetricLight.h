#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <array>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;
class GpuResource;

namespace VolumetricLight {
	class VolumetricLightClass {
	public:
		VolumetricLightClass();
		virtual ~VolumetricLightClass() = default;

	public:
		BOOL Initialize(
			ID3D12Device* device, ShaderManager* const manager,
			UINT width, UINT height, UINT depth);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			UINT descSize);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mVolumetricLightMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhVolumetricLightMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhVolumetricLightMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhVolumetricLightMapCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhVolumetricLightMapGpuUav;

		UINT mWidth;
		UINT mHeight;
		UINT mDepth;
	};
}