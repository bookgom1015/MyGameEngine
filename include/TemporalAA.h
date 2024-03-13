#pragma once

#include <d3dx12.h>
#include <array>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;
class GpuResource;

namespace TemporalAA {
	namespace RootSignature {
		enum {
			ESI_Input = 0,
			ESI_History,
			ESI_Velocity,
			ESI_Factor,
			Count
		};
	}

	class TemporalAAClass {
	public:
		TemporalAAClass();
		virtual ~TemporalAAClass() = default;

	public:
		__forceinline GpuResource* HistoryMapResource();

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE HistoryMapSrv() const;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
			FLOAT factor);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			UINT descSize);
		BOOL OnResize(UINT width, UINT height);

	public:
		void BuildDescriptors();
		BOOL BuildResources(UINT width, UINT height);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mCopiedBackBuffer;
		std::unique_ptr<GpuResource> mHistoryMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhHistoryMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhHistoryMapGpuSrv;
	};
}

#include "TemporalAA.inl"