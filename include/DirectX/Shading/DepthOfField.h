#pragma once

#include <DirectX/d3dx12.h>
#include <wrl.h>
#include <unordered_map>

#include "Common/Util/Locker.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "DirectX/Shading/Samplers.h"

class ShaderManager;

namespace DepthOfField {
	namespace RootSignature {
		enum Type {
			E_CoC = 0,
			E_DoF,
			E_DoFBlur,
			E_FD,
			Count
		};

		namespace CircleOfConfusion {
			enum {
				ECB_DoF = 0,
				ESI_Depth,
				EUI_FocalDist,
				Count
			};
		}

		namespace FocalDistance {
			enum {
				ECB_DoF = 0,
				ESI_Depth,
				EUO_FocalDist,
				Count
			};
		}

		namespace ApplyDoF {
			enum {
				EC_Consts = 0,
				ESI_BackBuffer,
				ESI_CoC,
				Count
			};
		}

		namespace BlurDoF {
			enum {
				ECB_Blur = 0,
				ESI_Input,
				ESI_CoC,
				Count
			};
		}
	}

	namespace PipelineState {
		enum Type {
			EG_CoC = 0,
			EG_DoF,
			EG_DoFBlur,
			EG_FD,
			Count
		};
	}

	const UINT NumRenderTargets = 1;

	class DepthOfFieldClass {
	public:
		DepthOfFieldClass();
		virtual ~DepthOfFieldClass() = default;

	public:		
		__forceinline GpuResource* CoCMapResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE CoCMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE CoCMapRtv() const;

		__forceinline GpuResource* FocalDistanceBufferResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE FocalDistanceBufferUav() const;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height);
		BOOL PrepareUpdate(ID3D12GraphicsCommandList* const cmdList);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		void CalcFocalDist(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS cb_dof,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);
		void CalcCoC(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS cb_dof,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);
		void ApplyDoF(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			FLOAT bokehRadius,
			FLOAT cocMaxDevTolerance,
			FLOAT highlightPower,
			INT sampleCount);
		void BlurDoF(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			UINT blurCount);

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

		std::unique_ptr<GpuResource> mCoCMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCoCMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCoCMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCoCMapCpuRtv;

		std::unique_ptr<GpuResource> mCopiedBackBuffer;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferGpuSrv;

		std::unique_ptr<GpuResource> mFocalDistanceBuffer;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhFocalDistanceCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhFocalDistanceGpuUav;

		BYTE* mMappedBuffer;
	};
}

#include "DepthOfField.inl"