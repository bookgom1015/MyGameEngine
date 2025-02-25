#pragma once

#include <DirectX/d3dx12.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>

#include "Common/Util/Locker.h"
#include "DirectX/Util/MipmapGenerator.h"
#include "DirectX/Shading/Samplers.h"

#include "HlslCompaction.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace EquirectangularConverter {
	namespace RootSignature {
		enum {
			E_ConvEquirectToCube = 0,
			E_ConvCubeToEquirect,
			Count
		};

		namespace ConvEquirectToCube {
			enum {
				ECB_ConvEquirectToCube = 0,
				ESI_Equirectangular,
				Count
			};
		}

		namespace ConvCubeToEquirect {
			enum {
				EC_Consts = 0,
				ESI_Cube,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
			EG_ConvEquirectToCube = 0,
			EG_ConvCubeToEquirect,
			Count
		};
	}

	class EquirectangularConverterClass {
	public:
		EquirectangularConverterClass() = default;
		virtual ~EquirectangularConverterClass() = default;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		void ConvertEquirectangularToCube(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const resource,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_output);
		void ConvertEquirectangularToCube(
			ID3D12GraphicsCommandList* const cmdList,
			UINT width, UINT height,
			GpuResource* const resource,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
			CD3DX12_CPU_DESCRIPTOR_HANDLE ro_outputs[],
			UINT maxMipLevel);
		void ConvertCubeToEquirectangular(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const equirectResource,
			D3D12_CPU_DESCRIPTOR_HANDLE equirectRtv,
			D3D12_GPU_DESCRIPTOR_HANDLE cubeSrv,
			UINT mipLevel = 0);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, RootSignature::Count> mRootSignatures;
		std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineState::Count> mPSOs;
	};
};