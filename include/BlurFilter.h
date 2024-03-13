#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"
#include "HlslCompaction.h"

class ShaderManager;

namespace BlurFilter {
	namespace RootSignature {
		enum {
			ECB_BlurPass = 0,
			EC_Consts,
			ESI_Normal,
			ESI_Depth,
			ESI_Input_F4,
			ESI_Input_F3,
			ESI_Input_F2,
			ESI_Input_F1,
			Count
		};

		namespace RootConstant {
			enum {
				E_Horizontal = 0,
				E_Bilateral,
				Count
			};
		}
	}

	class BlurFilterClass {
	public:
		BlurFilterClass() = default;
		virtual ~BlurFilterClass() = default;

	public:
		BOOL Initialize(ID3D12Device*const device, ShaderManager*const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
			GpuResource*const primary,
			GpuResource*const secondary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_primary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_secondary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
			FilterType type,
			UINT blurCount = 3);
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			GpuResource* const primary,
			GpuResource* const secondary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_primary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_secondary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
			FilterType type,
			UINT blurCount = 3);

	private:
		void Blur(
			ID3D12GraphicsCommandList* cmdList,
			GpuResource*const output,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_output,
			D3D12_GPU_DESCRIPTOR_HANDLE si_input,
			FilterType type,
			BOOL horzBlur);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<FilterType, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	};
}