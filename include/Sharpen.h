#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace Sharpen {
	namespace RootSignatureLayout {
		enum {
			EC_Consts = 0,
			ESI_BackBuffer,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			E_InvTexSizeX = 0,
			E_InvTexSizeY,
			E_Amount,
			Count
		};
	}

	class SharpenClass {
	public:
		SharpenClass();
		virtual ~SharpenClass() = default;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();
		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			UINT descSize);
		bool OnResize(UINT width, UINT height);

		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			float amount);

	private:
		void BuildDescriptors();
		bool BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mCopiedBackBuffer;
		D3D12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferCpuSrv;
		D3D12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferGpuSrv;

		UINT mWidth;
		UINT mHeight;
	};
}