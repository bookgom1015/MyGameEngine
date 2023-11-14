#pragma once

#include <d3dx12.h>
#include <array>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace Ssr {
	namespace RootSignatureLayout {
		enum {
			ECB_Ssr = 0,
			ESI_BackBuffer,
			ESI_Normal,
			ESI_Depth,
			Count
		};
	}

	static const UINT NumRenderTargets = 2;
	
	const float ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	class SsrClass {
	public:
		SsrClass();
		virtual ~SsrClass() = default;

	public:
		__forceinline GpuResource* SsrMapResource(UINT index);

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SsrMapSrv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SsrMapRtv(UINT index) const;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, 
			UINT width, UINT height, UINT divider);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();
		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		bool OnResize(UINT width, UINT height);

		void Build(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);

	private:
		void BuildDescriptors();
		bool BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		UINT mReducedWidth;
		UINT mReducedHeight;

		UINT mDivider;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::array<std::unique_ptr<GpuResource>, 2> mSsrMaps;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhSsrMapCpuSrvs;
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> mhSsrMapGpuSrvs;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhSsrMapCpuRtvs;
	};
}

#include "Ssr.inl"