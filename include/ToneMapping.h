#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace ToneMapping {
	namespace RootSignatureLayout {
		enum {
			EC_Cosnts = 0,
			ESI_Intermediate,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			E_Exposure = 0,
			Count
		};
	}

	namespace PipelineState {
		enum Type {
			E_ToneMapping = 0,
			E_JustResolving,
			Count
		};
	}

	static const UINT NumRenderTargets = 1;

	const FLOAT ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	class ToneMappingClass {
	public:
		ToneMappingClass();
		virtual ~ToneMappingClass() = default;

	public:
		__forceinline GpuResource* InterMediateMapResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE InterMediateMapSrv() const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE InterMediateMapRtv() const;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();

		void Resolve(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer);
		void Resolve(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			FLOAT exposure);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv, 
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv, 
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		BOOL BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		UINT mWidth;
		UINT mHeight;

		std::unique_ptr<GpuResource> mIntermediateMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhIntermediateMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateMapCpuRtv;
	};
}

GpuResource* ToneMapping::ToneMappingClass::InterMediateMapResource() {
	return mIntermediateMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ToneMapping::ToneMappingClass::InterMediateMapSrv() const {
	return mhIntermediateMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ToneMapping::ToneMappingClass::InterMediateMapRtv() const {
	return mhIntermediateMapCpuRtv;
}