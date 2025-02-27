#pragma once

#include <DirectX/d3dx12.h>
#include <array>
#include <unordered_map>
#include <wrl.h>

#include "Common/Util/Locker.h"

#include "Samplers.h"

class ShaderManager;

namespace GaussianFilter {
	namespace RootSignature {
		namespace Default {
			enum {
				EC_Consts = 0,
				ESI_Input,
				EUO_Output,
				Count
			};
		}
	}

	enum FilterType {
		Filter3x3 = 0,
		Filter3x3RG,
		Count
	};

	class GaussianFilterClass {
	public:
		GaussianFilterClass() = default;
		virtual ~GaussianFilterClass() = default;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_DESCRIPTOR_HANDLE si_input,
			D3D12_GPU_DESCRIPTOR_HANDLE uo_output,
			FilterType type,
			UINT width, UINT height);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<FilterType, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	};
}