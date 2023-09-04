#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <array>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;
class GpuResource;

namespace Bloom {
	namespace ApplyBloom {
		namespace RootSignatureLayout {
			enum {
				ESI_BackBuffer = 0,
				ESI_Bloom,
				Count
			};
		}
	}

	namespace ExtractHighlights {
		namespace RootSignatureLayout {
			enum {
				ESI_BackBuffer = 0,
				EC_Consts,
				Count
			};
		}

		namespace RootConstatLayout {
			enum {
				EThreshold = 0,
				Count
			};
		}
	}

	namespace PipelineState {
		enum Type {
			E_Extract = 0,
			E_Bloom,
			Count
		};
	}

	static const UINT NumRenderTargets = 3;

	const float ClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	class BloomClass {
	public:
		BloomClass();
		virtual ~BloomClass() = default;

	public:
		__forceinline GpuResource* BloomMapResource(UINT index);
		__forceinline GpuResource* ResultMapResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE BloomMapSrv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BloomMapRtv(UINT index) const;

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ResultMapRtv() const;

	public:
		bool Initialize(
			ID3D12Device* device, ShaderManager*const manager, 
			UINT width, UINT height, UINT divider, DXGI_FORMAT hdrMapFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout);
		void ExtractHighlights(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			float threshold);
		void Bloom(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer);

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

		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		UINT mBloomMapWidth;
		UINT mBloomMapHeight;

		UINT mResultMapWidth;
		UINT mResultMapHeight;

		UINT mDivider;

		D3D12_VIEWPORT mReducedViewport;
		D3D12_RECT mReducedScissorRect;

		D3D12_VIEWPORT mOriginalViewport;
		D3D12_RECT mOriginalScissorRect;

		DXGI_FORMAT mHDRMapFormat;

		std::array<std::unique_ptr<GpuResource>, 2> mBloomMaps;
		std::unique_ptr<GpuResource> mResultMap;

		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhBloomMapCpuSrvs;
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> mhBloomMapGpuSrvs;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhBloomMapCpuRtvs;
		
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhResultMapCpuRtv;
	};
}

GpuResource* Bloom::BloomClass::BloomMapResource(UINT index) {
	return mBloomMaps[index].get();
}

GpuResource* Bloom::BloomClass::ResultMapResource() {
	return mResultMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Bloom::BloomClass::BloomMapSrv(UINT index) const {
	return mhBloomMapGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::BloomClass::BloomMapRtv(UINT index) const {
	return mhBloomMapCpuRtvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::BloomClass::ResultMapRtv() const {
	return mhResultMapCpuRtv;
}