#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace MipmapGenerator {
	namespace RootSignature {
		namespace Default {
			enum {
				EC_Consts = 0,
				ESI_Input,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
			EG_GenerateMipmap = 0,
			EG_JustCopy,
			Count
		};
	}

	class MipmapGeneratorClass {
	public:
		MipmapGeneratorClass() = default;
		virtual ~MipmapGeneratorClass() = default;

	public:
		BOOL Initialize(ID3D12Device* const device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		BOOL GenerateMipmap(
			ID3D12GraphicsCommandList* const cmdList,
			GpuResource* const output,
			D3D12_GPU_DESCRIPTOR_HANDLE si_input,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_outputs[],
			UINT width, UINT height,
			UINT maxMipLevel);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineState::Count> mPSOs;
	};
}