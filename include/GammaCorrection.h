#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace GammaCorrection {
	namespace RootSignatureLayout {
		enum {
			EC_Consts = 0,
			ESI_BackBuffer,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			E_Gamma = 0,
			Count
		};
	}

	class GammaCorrectionClass {
	public:
		GammaCorrectionClass();
		virtual ~GammaCorrectionClass() = default;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			FLOAT gamma);

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,UINT descSize);
		BOOL OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		BOOL BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		std::unique_ptr<GpuResource> mDuplicatedBackBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDuplicatedBackBufferCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDuplicatedBackBufferGpuSrv;
	};
}