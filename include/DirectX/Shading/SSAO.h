#pragma once

#include <DirectX/d3dx12.h>
#include <array>

#include "Common/Helper/MathHelper.h"
#include "Common/Util/Locker.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "DirectX/Shading/Samplers.h"

class ShaderManager;

namespace SSAO {
	namespace RootSignature {
		enum {
			ECB_Pass = 0,
			ESI_Normal,
			ESI_Depth,
			ESI_RandomVector,
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

	const UINT NumRenderTargets = 2;

	class SSAOClass {
	public:
		SSAOClass();
		virtual ~SSAOClass() = default;

	public:
		__forceinline constexpr D3D12_VIEWPORT Viewport() const;
		__forceinline constexpr D3D12_RECT ScissorRect() const;

		__forceinline GpuResource* AOCoefficientMapResource(UINT index);
		__forceinline GpuResource* RandomVectorMapResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE AOCoefficientMapSrv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE AOCoefficientMapRtv(UINT index) const;

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE RandomVectorMapSrv() const;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height, Resolution::Type type);
		BOOL PrepareUpdate(ID3D12GraphicsCommandList* const cmdList);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);

		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL BuildDescriptors();
		BOOL OnResize(UINT width, UINT height);

	private:
		BOOL BuildResources(UINT width, UINT height);

		void BuildOffsetVectors();
		BOOL BuildRandomVectorTexture(ID3D12GraphicsCommandList* const cmdList);
		
	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		Resolution::Type mResolutionType;
				
		std::unique_ptr<GpuResource> mRandomVectorMap;
		std::unique_ptr<GpuResource> mRandomVectorMapUploadBuffer;
		std::array<std::unique_ptr<GpuResource>, 2> mAOCoefficientMaps;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

		// Need two for ping-ponging during blur.
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhAOCoefficientMapCpuSrvs;
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 2> mhAOCoefficientMapGpuSrvs;
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 2> mhAOCoefficientMapCpuRtvs;

		DirectX::XMFLOAT4 mOffsets[14];

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;
	};
}

#include "SSAO.inl"