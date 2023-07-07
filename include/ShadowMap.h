#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

struct RenderItem;

namespace ShadowMap {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ECB_Obj,
			ECB_Mat,
			ESI_TexMaps,
			Count
		};
	}

	const UINT NumDepthStenciles = 1;

	class ShadowMapClass {
	public:
		ShadowMapClass();
		virtual ~ShadowMapClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline constexpr D3D12_VIEWPORT Viewport() const;
		__forceinline constexpr D3D12_RECT ScissorRect() const;

		__forceinline GpuResource* Resource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat);
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress,
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
			const std::vector<RenderItem*>& ritems);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
			UINT descSize, UINT dsvDescSize);

	private:
		void BuildDescriptors();
		bool BuildResources();

		void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress, D3D12_GPU_VIRTUAL_ADDRESS matCBAddress);

	protected:
		const DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::unique_ptr<GpuResource> mShadowMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;
	};
};

#include "ShadowMap.inl"