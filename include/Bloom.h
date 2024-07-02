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
	namespace RootSignature {
		namespace ApplyBloom {
			enum {
				ESI_BackBuffer = 0,
				ESI_Bloom,
				Count
			};
		}

		namespace HighlightExtraction {
			enum {
				ESI_BackBuffer = 0,
				EC_Consts,
				Count
			};

			namespace RootConstant {
				enum {
					EThreshold = 0,
					Count
				};
			}
		}
	}

	namespace PipelineState {
		enum Type {
			E_Extract = 0,
			E_Bloom,
			Count
		};
	}

	namespace Resolution {
		enum Type {
			E_Fullscreen = 0,
			E_Quarter,
			E_OneSixteenth,
			Count
		};
	}

	static const UINT NumRenderTargets = 2;

	const FLOAT ClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	class BloomClass {
	public:
		BloomClass();
		virtual ~BloomClass() = default;

	public:
		__forceinline GpuResource* BloomMapResource(UINT index);

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE BloomMapSrv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BloomMapRtv(UINT index) const;

	public:
		BOOL Initialize(
			ID3D12Device* device, ShaderManager*const manager, 
			UINT width, UINT height, Resolution::Type type);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void ExtractHighlight(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			FLOAT threshold);
		void ApplyBloom(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource*const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL OnResize(UINT width, UINT height);

	public:
		void BuildDescriptors();
		BOOL BuildResources(UINT width, UINT height);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		Resolution::Type mResolutionType;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::array<std::unique_ptr<GpuResource>, 2> mBloomMaps;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhBloomMapCpuSrvs;
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> mhBloomMapGpuSrvs;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhBloomMapCpuRtvs;
		
		std::unique_ptr<GpuResource> mCopiedBackBuffer;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferGpuSrv;
	};
}

#include "Bloom.inl"