#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace MipmapGenerator {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			EC_Consts,
			ESI_Input,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			E_InvTexSizeW = 0,
			E_InvTexSizeH,
			E_InvMipmapTexSizeW,
			E_InvMipmapTexSizeH,
			Count
		};
	}

	namespace PipelineState {
		enum Type {
			E_GenerateMipmap = 0,
			E_JustCopy,
			Count
		};
	}

	class MipmapGeneratorClass {
	public:
		MipmapGeneratorClass() = default;
		virtual ~MipmapGeneratorClass() = default;

	public:
		BOOL Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();

		BOOL GenerateMipmap(
			ID3D12GraphicsCommandList* const cmdList,
			GpuResource* output,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_DESCRIPTOR_HANDLE si_input,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_outputs[],
			UINT width, UINT height,
			UINT maxMipLevel);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	};
}