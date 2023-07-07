#pragma once

#include <d3dx12.h>
#include <array>

#include "MathHelper.h"
#include "FrameResource.h"
#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace Ssao {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ESI_Normal,
			ESI_Depth,
			ESI_RandomVector,
			Count
		};
	}

	const UINT NumRenderTargets = 2;

	const DXGI_FORMAT AOCoefficientMapFormat = DXGI_FORMAT_R16_UNORM;
	const DXGI_FORMAT RandomVectorMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	const float AOCoefficientMapClearValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	class SsaoClass {
	public:
		SsaoClass();
		virtual ~SsaoClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline constexpr D3D12_VIEWPORT Viewport() const;
		__forceinline constexpr D3D12_RECT ScissorRect() const;

		__forceinline GpuResource* AOCoefficientMapResource(UINT index);
		__forceinline GpuResource* RandomVectorMapResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE AOCoefficientMapSrv(UINT index) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE AOCoefficientMapRtv(UINT index) const;

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE RandomVectorMapSrv() const;

	public:
		bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ShaderManager*const manager,
			UINT width, UINT height, UINT divider);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout);
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth);

		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		bool OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		bool BuildResources();

		void BuildOffsetVectors();
		bool BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);
		
	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		UINT mDivider;
		
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

#include "Ssao.inl"