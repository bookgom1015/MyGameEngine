#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "MathHelper.h"
#include "HlslCompaction.h"

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace DebugMap {
	namespace RootSignatureLayout {
		enum {
			ECB_DebugMap = 0,
			EC_Consts,
			ESI_Debug0,
			ESI_Debug1,
			ESI_Debug2,
			ESI_Debug3,
			ESI_Debug4,
			ESI_Debug0_uint,
			ESI_Debug1_uint,
			ESI_Debug2_uint,
			ESI_Debug3_uint,
			ESI_Debug4_uint,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			ESampleMask0 = 0,
			ESampleMask1,
			ESampleMask2,
			ESampleMask3,
			ESampleMask4,
			Count
		};
	}

	class DebugMapClass {
	public:
		DebugMapClass() = default;
		virtual ~DebugMapClass() = default;

	public:
		constexpr __forceinline DebugMapSampleDesc SampleDesc(UINT index) const;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager*const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_debug,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv);

		BOOL AddDebugMap(
			D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv, 
			SampleMask::Type mask = SampleMask::RGB);
		BOOL AddDebugMap(
			D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
			SampleMask::Type mask,
			DebugMapSampleDesc desc);
		void RemoveDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::array<D3D12_GPU_DESCRIPTOR_HANDLE, MapSize> mhDebugGpuSrvs;
		std::array<SampleMask::Type, MapSize> mDebugMasks;
		std::array<DebugMapSampleDesc, MapSize> mSampleDescs;
		INT mNumEnabledMaps;
	};
};

constexpr DebugMapSampleDesc DebugMap::DebugMapClass::SampleDesc(UINT index) const {
	return mSampleDescs[index];
}