#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

struct RenderItem;

namespace GBuffer {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ECB_Obj,
			ECB_Mat,
			ESI_TexMaps,
			Count
		};
	}

	static const UINT NumRenderTargets = 6;

	const FLOAT AlbedoMapClearValues[4]				= { 0.0f, 0.0f, 0.0f, 0.0f };
	const FLOAT NormalMapClearValues[4]				= { 0.0f, 0.0f, 0.0f, 0.0f };
	const FLOAT NormalDepthMapClearValues[4]		= { 0.0f, 0.0f, 0.0f, 0.0f };
	const FLOAT RMSMapClearValues[4]				= { 0.5f, 0.0f, 0.5f, 0.0f };
	const FLOAT VelocityMapClearValues[2]			= { 1000.0f, 1000.0f };
	const FLOAT ReprojNormalDepthMapClearValues[4]	= { 0.0f, 0.0f, 0.0f, 0.0f };

	class GBufferClass {
	public:
		GBufferClass();
		virtual ~GBufferClass() = default;

	public:
		__forceinline GpuResource* AlbedoMapResource();
		__forceinline GpuResource* NormalMapResource();
		__forceinline GpuResource* NormalDepthMapResource();
		__forceinline GpuResource* RMSMapResource();
		__forceinline GpuResource* VelocityMapResource();
		__forceinline GpuResource* ReprojNormalDepthMapResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE AlbedoMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE NormalDepthMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE RMSMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE VelocityMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ReprojNormalDepthMapSrv() const;

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE AlbedoMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE NormalDepthMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE RMSMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE VelocityMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ReprojNormalDepthMapRtv() const;

	public:
		BOOL Initialize(ID3D12Device*const device, UINT width, UINT height, ShaderManager*const manager, 
			GpuResource*const depth, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress,
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
			const std::vector<RenderItem*>& ritems);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		BOOL BuildResources(UINT width, UINT height);

		void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress, D3D12_GPU_VIRTUAL_ADDRESS matCBAddress);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mAlbedoMap;
		std::unique_ptr<GpuResource> mNormalMap;
		std::unique_ptr<GpuResource> mNormalDepthMap;
		std::unique_ptr<GpuResource> mRMSMap;
		std::unique_ptr<GpuResource> mVelocityMap;
		std::unique_ptr<GpuResource> mReprojNormalDepthMap;

		GpuResource* mDepthMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuDsv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAlbedoMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhAlbedoMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAlbedoMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalDepthMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalDepthMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalDepthMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhRMSMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhRMSMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhRMSMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhVelocityMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhVelocityMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhVelocityMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReprojNormalDepthMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhReprojNormalDepthMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReprojNormalDepthMapCpuRtv;
	};
}

#include "GBuffer.inl"