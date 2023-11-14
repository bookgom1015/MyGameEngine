#pragma once

#include <d3dx12.h>
#include <wrl.h>
#include <unordered_map>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace DepthOfField {
	namespace CircleOfConfusion {
		namespace RootSignatureLayout {
			enum {
				ECB_Dof = 0,
				ESI_Depth,
				EUI_FocalDist,
				Count
			};
		}
	}

	namespace Bokeh {
		namespace RootSignatureLayout {
			enum {
				ESI_Input = 0,
				EC_Consts,
				Count
			};
		}

		namespace RootConstantsLayout {
			enum {
				EBokehRadius = 0,
				Count
			};
		}
	}

	namespace FocalDistance {
		namespace RootSignatureLayout {
			enum {
				ECB_Dof = 0,
				ESI_Depth,
				EUO_FocalDist,
				Count
			};
		}
	}

	namespace ApplyingDof {
		namespace RootSignatureLayout {
			enum {
				EC_Consts = 0,
				ESI_BackBuffer,
				ESI_Coc,
				Count
			};
		}

		namespace RootConstantLayout {
			enum {
				EBokehRadius = 0,
				ECocThreshold,
				ECocDiffThreshold,
				EHighlightPower,
				ENumSamples,
				Count
			};
		}
	}

	namespace DofBlur {
		namespace RootSignatureLayout {
			enum {
				ECB_Blur = 0,
				EC_Consts,
				ESI_Input,
				ESI_Coc,
				Count
			};
		}

		namespace RootConstantLayout {
			enum {
				EHorizontalBlur = 0,
				Count
			};
		}
	}


	const UINT NumRenderTargets = 3;

	const DXGI_FORMAT CocMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;

	const float CocMapClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	const float BokehMapClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const float DofMapClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	class DepthOfFieldClass {
	public:
		DepthOfFieldClass();
		virtual ~DepthOfFieldClass() = default;

	public:
		__forceinline constexpr UINT CocMapWidth() const;
		__forceinline constexpr UINT CocMapHeight() const;

		__forceinline constexpr UINT DofMapWidth() const;
		__forceinline constexpr UINT DofMapHeight() const;
		
		__forceinline GpuResource* CocMapResource();
		__forceinline GpuResource* DofMapResource();
		__forceinline GpuResource* FocalDistanceBufferResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE CocMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE CocMapRtv() const;

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DofMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DofMapRtv() const;

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE FocalDistanceBufferUav() const;

	public:
		bool Initialize(
			ID3D12Device* device, ShaderManager*const manager, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();

		void CalcFocalDist(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);
		void CalcCoc(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);
		void ApplyDof(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			float bokehRadius,
			float cocThreshold,
			float cocDiffThreshold,
			float highlightPower,
			int numSamples);
		void BlurDof(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			UINT blurCount);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		bool OnResize(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);

	public:
		void BuildDescriptors();
		bool BuildResources(ID3D12GraphicsCommandList* cmdList);

		void Blur(ID3D12GraphicsCommandList*const cmdList, bool horzBlur);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::unique_ptr<GpuResource> mCocMap;
		std::array<std::unique_ptr<GpuResource>, 2> mDofMaps;
		std::unique_ptr<GpuResource> mFocalDistanceBuffer;

		BYTE* mMappedBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCocMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCocMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCocMapCpuRtv;

		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhDofMapCpuSrvs;
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> mhDofMapGpuSrvs;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhDofMapCpuRtvs;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhFocalDistanceCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhFocalDistanceGpuUav;
	};
}

#include "DepthOfField.inl"