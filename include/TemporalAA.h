#pragma once

#include <d3dx12.h>
#include <array>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;
class GpuResource;

namespace TemporalAA {
	namespace RootSignatureLayout {
		enum {
			ESI_Input = 0,
			ESI_History,
			ESI_Velocity,
			ESI_Factor,
			Count
		};
	}

	const UINT NumRenderTargets = 1;

	const FLOAT ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	class TemporalAAClass {
	public:
		TemporalAAClass();
		virtual ~TemporalAAClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline GpuResource* ResolveMapResource();
		__forceinline GpuResource* HistoryMapResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ResolveMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ResolveMapRtv() const;

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE HistoryMapSrv() const;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();

		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			GpuResource* backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
			FLOAT factor);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL OnResize(UINT width, UINT height);

	public:
		void BuildDescriptors();
		BOOL BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::unique_ptr<GpuResource> mResolveMap;
		std::unique_ptr<GpuResource> mHistoryMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhResolveMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhResolveMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhResolveMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhHistoryMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhHistoryMapGpuSrv;

		BOOL bInitiatingTaa;
	};
}

#include "TemporalAA.inl"