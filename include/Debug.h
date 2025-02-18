#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "MathHelper.h"
#include "HlslCompaction.h"

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace Debug {
	namespace RootSignature {
		enum {
			E_DebugTexMap = 0,
			E_DebugCubeMap,
			Count
		};

		namespace DebugTexMap {
			enum {
				ECB_Debug = 0,
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

		namespace DebugCubeMap {
			enum {
				ECB_Pass = 0,
				EC_Consts,
				ESI_Cube,
				ESI_Equirectangular,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
			EG_DebugTexMap = 0,
			EG_DebugCubeMap,
			EG_DebugEquirectangularMap,
			Count
		};
	}

	class DebugClass {
	public:
		DebugClass() = default;
		virtual ~DebugClass() = default;

	public:
		constexpr __forceinline DebugSampleDesc SampleDesc(UINT index) const;

	public:
		BOOL Initialize(ID3D12Device* const device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		BOOL AddDebugMap(
			D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv, 
			SampleMask::Type mask = SampleMask::RGB);
		BOOL AddDebugMap(
			D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
			SampleMask::Type mask,
			DebugSampleDesc desc);
		void RemoveDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv);

		void DebugTexMap(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_debug,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv);

		BOOL DebugCubeMap(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_input,
			FLOAT mipLevel = 0.0f);

	public:
		IrradianceMap::CubeMap::Type DebugCubeMapType = IrradianceMap::CubeMap::E_EnvironmentCube;

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, RootSignature::Count> mRootSignatures;
		std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineState::Count> mPSOs;

		std::array<D3D12_GPU_DESCRIPTOR_HANDLE, MapSize> mhDebugGpuSrvs;
		std::array<SampleMask::Type, MapSize> mDebugMasks;
		std::array<DebugSampleDesc, MapSize> mSampleDescs;
		UINT mNumEnabledMaps;
	};
};

#include "Debug.inl"