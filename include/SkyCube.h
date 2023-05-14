#pragma once

#include <d3dx12.h>

#include "Samplers.h"

class ShaderManager;

struct RenderItem;

namespace SkyCube {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ESI_Cube,
			Count
		};
	}

	class SkyCubeClass {
	public:
		SkyCubeClass() = default;
		virtual ~SkyCubeClass() = default;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT backBufferFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat);
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_cube,
			const std::vector<RenderItem*>& ritems);

		bool OnResize(UINT width, UINT height);

	private:
		void DrawRenderItems(ID3D12GraphicsCommandList*const cmdList, const std::vector<RenderItem*>& ritems);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		DXGI_FORMAT mBackBufferFormat;
	};
}