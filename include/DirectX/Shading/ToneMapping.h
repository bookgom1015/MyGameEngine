#pragma once

#include <DirectX/d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Common/Util/Locker.h"

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace ToneMapping {
	namespace RootSignature {
		namespace Default {
			enum {
				EC_Cosnts = 0,
				ESI_Intermediate,
				Count
			};
		}
	}

	namespace PipelineState {
		enum Type {
			E_ToneMapping = 0,
			E_JustResolving,
			Count
		};
	}

	static const UINT NumRenderTargets = 1;

	class ToneMappingClass {
	public:
		ToneMappingClass();
		virtual ~ToneMappingClass() = default;

	public:
		__forceinline GpuResource* InterMediateMapResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE InterMediateMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE InterMediateMapRtv() const;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		void Resolve(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer);
		void Resolve(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			FLOAT exposure);

		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv, 
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv, 
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL BuildDescriptors();
		BOOL OnResize(UINT width, UINT height);

	private:
		BOOL BuildResources(UINT width, UINT height);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		std::unique_ptr<GpuResource> mIntermediateMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhIntermediateMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateMapCpuRtv;
	};
}

#include "ToneMapping.inl"