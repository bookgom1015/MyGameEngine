#pragma once

#include <d3dx12.h>
#include <array>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;

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

	const float ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	class TemporalAAClass {
	public:
		TemporalAAClass() = default;
		virtual ~TemporalAAClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline ID3D12Resource* ResolveMapResource();
		__forceinline ID3D12Resource* HistoryMapResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ResolveMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ResolveMapRtv() const;

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE HistoryMapSrv() const;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT format);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();

		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			ID3D12Resource* backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
			float factor);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		bool OnResize(UINT width, UINT height);

	public:
		void BuildDescriptors();
		bool BuildResource();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		DXGI_FORMAT mBackBufferFormat;

		Microsoft::WRL::ComPtr<ID3D12Resource> mResolveMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mHistoryMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhResolveMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhResolveMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhResolveMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhHistoryMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhHistoryMapGpuSrv;

		bool bInitiatingTaa;
	};
}

#include "TemporalAA.inl"