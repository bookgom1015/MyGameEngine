#pragma once

#include <DirectX/d3dx12.h>
#include <wrl.h>

#include "Common/Helper/MathHelper.h"
#include "Common/Util/Locker.h"
#include "DirectX/Shading/Samplers.h"

#include "HlslCompaction.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace Debug {
	namespace RootSignature {
		enum {
			E_DebugTexMap = 0,
			E_DebugCubeMap,
			E_Collsion,
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

		namespace Collision {
			enum {
				ECB_Pass = 0,
				ECB_Obj,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
			EG_DebugTexMap = 0,
			EG_DebugCubeMap,
			EG_DebugEquirectangularMap,
			EG_Collision,
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
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager);
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

		void ShowCollsion(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
			UINT objCBByteSize,
			const std::vector<RenderItem*>& ritems);

	public:
		IrradianceMap::CubeMap::Type DebugCubeMapType = IrradianceMap::CubeMap::E_EnvironmentCube;

	private:
		Locker<ID3D12Device5>* md3dDevice;
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