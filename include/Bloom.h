#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <array>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"
#include "Locker.h"

class ShaderManager;
class GpuResource;

namespace Bloom {
	namespace RootSignature {
		enum {
			E_ApplyBloom,
			E_ExtractHighlight,
			Count
		};

		namespace ApplyBloom {
			enum {
				ESI_BackBuffer = 0,
				ESI_Bloom,
				Count
			};
		}

		namespace ExtractHighlight {
			enum {
				ESI_BackBuffer = 0,
				EC_Consts,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
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
			Locker<ID3D12Device5>* const device, ShaderManager* const manager,
			UINT width, UINT height, Resolution::Type type);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void ExtractHighlight(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			FLOAT threshold);
		void ApplyBloom(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer);

		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL BuildDescriptors();
		BOOL OnResize(UINT width, UINT height);

	public:
		BOOL BuildResources(UINT width, UINT height);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, RootSignature::Count> mRootSignatures;
		std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineState::Count> mPSOs;

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