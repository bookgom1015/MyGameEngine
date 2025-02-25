#pragma once

#include <DirectX/d3dx12.h>
#include <wrl.h>

#include "Common/Util/Locker.h"

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace GammaCorrection {
	namespace RootSignature {
		namespace Default {
			enum {
				EC_Consts = 0,
				ESI_BackBuffer,
				Count
			};
		}
	}

	class GammaCorrectionClass {
	public:
		GammaCorrectionClass();
		virtual ~GammaCorrectionClass() = default;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			FLOAT gamma);

		void AllocateDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,UINT descSize);
		BOOL BuildDescriptors();
		BOOL OnResize(UINT width, UINT height);

	private:
		BOOL BuildResources(UINT width, UINT height);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mCopiedBackBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferGpuSrv;
	};
}