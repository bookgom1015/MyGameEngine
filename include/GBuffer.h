#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"

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

	static const UINT NumRenderTargets = 5;

	const DXGI_FORMAT ColorMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT AlbedoMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
	const DXGI_FORMAT DepthMapFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	const DXGI_FORMAT SpecularMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT VelocityMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;

	const float ColorMapClearValues[4]		= { 0.0f, 0.0f, 0.0f, 1.0f };
	const float AlbedoMapClearValues[4]		= { 0.0f, 0.0f, 0.0f, 0.0f };
	const float NormalMapClearValues[4]		= { 0.0f, 0.0f, 1.0f, 0.0f };
	const float SpecularMapClearValues[4]	= { 0.0f, 0.0f, 0.0f, 1.0f };
	const float VelocityMapClearValues[4]	= { 0.0f, 0.0f, 0.0f, 0.0f };

	class GBufferClass {
	public:
		GBufferClass() = default;
		virtual ~GBufferClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline ID3D12Resource* ColorMapResource();
		__forceinline ID3D12Resource* AlbedoMapResource();
		__forceinline ID3D12Resource* NormalMapResource();
		__forceinline ID3D12Resource* SpecularMapResource();
		__forceinline ID3D12Resource* VelocityMapResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ColorMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE AlbedoMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SpecularMapSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE VelocityMapSrv() const;

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ColorMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE AlbedoMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SpecularMapRtv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE VelocityMapRtv() const;

	public:
		bool Initialize(ID3D12Device* device, UINT width, UINT height, ShaderManager*const manager, 
			ID3D12Resource* depth, D3D12_CPU_DESCRIPTOR_HANDLE dsv, DXGI_FORMAT depthFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout);
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress,
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
			const std::vector<RenderItem*>& ritems);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize,
			ID3D12Resource* depth);
		bool OnResize(UINT width, UINT height, ID3D12Resource* depth);

	private:
		void BuildDescriptors(ID3D12Resource* depth);
		bool BuildResource();

		void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress, D3D12_GPU_VIRTUAL_ADDRESS matCBAddress);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		Microsoft::WRL::ComPtr<ID3D12Resource> mColorMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mAlbedoMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSpecularMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mVelocityMap;

		ID3D12Resource* mDepthMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuDsv;
		DXGI_FORMAT mDepthFormat;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhColorMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhColorMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhColorMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAlbedoMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhAlbedoMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhAlbedoMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhSpecularMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhVelocityMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhVelocityMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhVelocityMapCpuRtv;
	};
}

#include "GBuffer.inl"