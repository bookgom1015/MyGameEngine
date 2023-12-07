#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace BlurFilter {
	namespace RootSignatureLayout {
		enum {
			ECB_BlurPass = 0,
			EC_Consts,
			ESI_Normal,
			ESI_Depth,
			ESI_Input,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			EHorizontal = 0,
			EBilateral,
			Count
		};
	}

	enum FilterType {
		R8G8B8A8,
		R16,
		R16G16B16A16,
		R32G32B32A32,
		Count
	};

	class BlurFilterClass {
	public:
		BlurFilterClass() = default;
		virtual ~BlurFilterClass() = default;

	public:
		BOOL Initialize(ID3D12Device*const device, ShaderManager*const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			GpuResource*const primary,
			GpuResource*const secondary,
			D3D12_CPU_DESCRIPTOR_HANDLE primaryRtv,
			D3D12_GPU_DESCRIPTOR_HANDLE primarySrv,
			D3D12_CPU_DESCRIPTOR_HANDLE secondaryRtv,
			D3D12_GPU_DESCRIPTOR_HANDLE secondarySrv,
			FilterType type,
			size_t blurCount = 3);
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE normalSrv,
			D3D12_GPU_DESCRIPTOR_HANDLE depthSrv,
			GpuResource* const primary,
			GpuResource* const secondary,
			D3D12_CPU_DESCRIPTOR_HANDLE primaryRtv,
			D3D12_GPU_DESCRIPTOR_HANDLE primarySrv,
			D3D12_CPU_DESCRIPTOR_HANDLE secondaryRtv,
			D3D12_GPU_DESCRIPTOR_HANDLE secondarySrv,
			FilterType type,
			size_t blurCount = 3);

	private:
		void Blur(
			ID3D12GraphicsCommandList* cmdList,
			GpuResource*const output,
			D3D12_CPU_DESCRIPTOR_HANDLE outputRtv,
			D3D12_GPU_DESCRIPTOR_HANDLE inputSrv,
			BOOL horzBlur);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<FilterType, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	};
}