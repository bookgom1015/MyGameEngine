#pragma once

#include <DirectX/d3dx12.h>
#include <array>
#include <unordered_map>
#include <wrl.h>

#include "Common/Util/Locker.h"

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace SSR {
	namespace RootSignature {
		namespace Default {
			enum {
				ECB_SSR = 0,
				ESI_BackBuffer,
				ESI_Position,
				ESI_Normal,
				ESI_Depth,
				ESI_RMS,
				Count
			};
		}
	}

	namespace PipelineState {
		enum Type {
			EG_ViewSpace = 0,
			EG_ScreenSpace,
			Count
		};
	}

	namespace Resolution {
		enum Type {
			E_Fullscreen = 0,
			E_Quarter,
			Count
		};
	}

	static const UINT NumRenderTargets = 2;

	class SSRClass {
	public:
		SSRClass();
		virtual ~SSRClass() = default;

	public:
		__forceinline GpuResource* SSRMapResource(UINT index);

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SSRMapSrv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SSRMapRtv(UINT index) const;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager,
			UINT width, UINT height, Resolution::Type type);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL BuildDescriptors();
		BOOL OnResize(UINT width, UINT height);

		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			GpuResource* const backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_position,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_rms);

	private:
		BOOL BuildResources(UINT width, UINT height);

	public:
		PipelineState::Type StateType = PipelineState::EG_ScreenSpace;

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineState::Count> mPSOs;

		Resolution::Type mResolutionType;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::array<std::unique_ptr<GpuResource>, 2> mSSRMaps;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhSSRMapCpuSrvs;
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> mhSSRMapGpuSrvs;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhSSRMapCpuRtvs;
	};
}

#include "SSR.inl"