#pragma once

#include <d3dx12.h>
#include <array>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace Ssr {
	namespace Building {
		namespace RootSignatureLayout {
			enum {
				ECB_Ssr = 0,
				ESI_BackBuffer,
				ESI_Normal,
				ESI_Depth,
				ESI_Spec,
				Count
			};
		}
	}

	namespace Applying {
		namespace RootSignatureLayout {
			enum {
				ECB_Ssr = 0,
				ESI_Cube,
				ESI_BackBuffer,
				ESI_Normal,
				ESI_Depth,
				ESI_Spec,
				ESI_Ssr,
				Count
			};
		}
	}

	static const UINT NumRenderTargets = 3;
	
	const float ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	class SsrClass {
	public:
		SsrClass();
		virtual ~SsrClass() = default;

	public:
		__forceinline GpuResource* SsrMapResource(UINT index);
		__forceinline GpuResource* ResultMapResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SsrMapSrv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SsrMapRtv(UINT index) const;

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ResultMapRtv() const;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, 
			UINT width, UINT height, UINT divider, DXGI_FORMAT hdrMapFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();

		void BuildSsr(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_spec);
		void ApplySsr(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_cube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_spec);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		bool OnResize(UINT width, UINT height);

	public:
		void BuildDescriptors();
		bool BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		UINT mSsrMapWidth;
		UINT mSsrMapHeight;

		UINT mResultMapWidth;
		UINT mResultMapHeight;

		UINT mDivider;

		D3D12_VIEWPORT mOriginalViewport;
		D3D12_RECT mOriginalScissorRect;

		D3D12_VIEWPORT mReducedViewport;
		D3D12_RECT mReducedScissorRect;

		DXGI_FORMAT mHDRMapFormat;

		std::array<std::unique_ptr<GpuResource>, 2> mSsrMaps;
		std::unique_ptr<GpuResource> mResultMap;

		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhSsrMapCpuSrvs;
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> mhSsrMapGpuSrvs;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhSsrMapCpuRtvs;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhResultMapCpuRtv;
	};
}

GpuResource* Ssr::SsrClass::SsrMapResource(UINT index) {
	return mSsrMaps[index].get();
}

GpuResource* Ssr::SsrClass::ResultMapResource() {
	return mResultMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssr::SsrClass::SsrMapSrv(UINT index) const {
	return mhSsrMapGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssr::SsrClass::SsrMapRtv(UINT index) const {
	return mhSsrMapCpuRtvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssr::SsrClass::ResultMapRtv() const {
	return mhResultMapCpuRtv;
}