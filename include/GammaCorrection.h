#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace GammaCorrection {
	namespace RootSignature {
		enum {
			EC_Consts = 0,
			ESI_BackBuffer,
			Count
		};

		namespace RootConstant {
			enum {
				E_Gamma = 0,
				Count
			};
		}
	}

	class GammaCorrectionClass {
	public:
		GammaCorrectionClass();
		virtual ~GammaCorrectionClass() = default;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			FLOAT gamma);

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,UINT descSize);
		BOOL OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		BOOL BuildResources(UINT width, UINT height);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mCopiedBackBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferGpuSrv;
	};
}