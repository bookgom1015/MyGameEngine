#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"
#include "HlslCompaction.h"

class ShaderManager;

struct RenderItem;

namespace Shadow {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ECB_Obj,
			ECB_Mat,
			ESI_TexMaps,
			Count
		};
	}

	const UINT NumDepthStenciles = MaxLights;

	class ShadowClass {
	public:
		ShadowClass();
		virtual ~ShadowClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline constexpr D3D12_VIEWPORT Viewport() const;
		__forceinline constexpr D3D12_RECT ScissorRect() const;

		__forceinline GpuResource* Resource(UINT index);
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Srv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv(UINT index) const;

		__forceinline BOOL* DebugShadowMap(UINT index);

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
			D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
			UINT objCBByteSize,
			UINT matCBByteSize,
			D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
			const std::vector<RenderItem*>& ritems,
			UINT index);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
			UINT descSize, UINT dsvDescSize);

	private:
		void BuildDescriptors();
		BOOL BuildResources();

		void DrawRenderItems(
			ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS cb_obj, D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
			UINT objCBByteSize, UINT matCBByteSize);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::unique_ptr<GpuResource> mShadowMaps[MaxLights];

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrvs[MaxLights];
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrvs[MaxLights];
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsvs[MaxLights];

		// Debugging
		BOOL mDebugShadowMaps[MaxLights] = { FALSE };
	};
};

#include "Shadow.inl"