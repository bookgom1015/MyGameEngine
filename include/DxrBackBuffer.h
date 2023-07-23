#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"

namespace BackBuffer { class BackBufferClass; }

namespace DxrBackBuffer {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ESI_Color,
			ESI_Albedo,
			ESI_Normal,
			ESI_Depth,
			ESI_Specular,
			ESI_Shadow,
			ESI_AOCoeff,
			Count
		};
	}

	class DxrBackBufferClass {
	public:
		DxrBackBufferClass() = default;
		virtual ~DxrBackBufferClass() = default;

	public:
		bool Initialize(BackBuffer::BackBufferClass*const backBuffer);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			ID3D12Resource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ri_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_color,
			D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_specular,
			D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
			D3D12_GPU_DESCRIPTOR_HANDLE si_aoceoff);

	private:
		BackBuffer::BackBufferClass* mBackBuffer;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	};
}