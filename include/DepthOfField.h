#pragma once

#include <d3dx12.h>
#include <wrl.h>
#include <unordered_map>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace DepthOfField {
	namespace PipelineState {
		enum Type {
			E_CoC = 0,
			E_DoF,
			E_DoFBlur,
			E_FD,
			Count
		};
	}

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

		namespace ApplyingDoF {
			enum {
				EC_Consts = 0,
				ESI_BackBuffer,
				ESI_CoC,
				Count
			};

			namespace RootConstant {
				enum {
					E_BokehRadius = 0,
					E_MaxDevTolerance,
					E_HighlightPower,
					E_SampleCount,
					Count
				};
			}
		}

		namespace BlurringDoF {
			enum {
				ECB_Blur = 0,
				EC_Consts,
				ESI_Input,
				ESI_CoC,
				Count
			};

			namespace RootConstant {
				enum {
					E_Horz = 0,
					Count
				};
			}
		}
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
		BOOL Initialize(
			ID3D12Device* device, ShaderManager*const manager, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();

		void CalcFocalDist(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS cb_dof,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);
		void CalcCoC(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS cb_dof,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);
		void ApplyDoF(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			FLOAT bokehRadius,
			FLOAT cocMaxDevTolerance,
			FLOAT highlightPower,
			INT numSamples);
		void BlurDoF(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			UINT blurCount);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL OnResize(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);

	public:
		void BuildDescriptors();
		BOOL BuildResources(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignature::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

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