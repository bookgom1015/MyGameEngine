#pragma once

#include <d3dx12.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"
#include "MipmapGenerator.h"
#include "HlslCompaction.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace EquirectangularConverter {
	namespace RootSignature {
		enum Type {
			E_ConvEquirectToCube = 0,
			E_ConvCubeToEquirect,
			Count
		};

		namespace ConvEquirectToCube {
			enum {
				ECB_ConvEquirectToCube = 0,
				EC_Consts,
				ESI_Equirectangular,
				Count
			};

			namespace RootConstant {
				enum {
					E_FaceID = 0,
					Count
				};
			}
		}

		namespace ConvCubeToEquirect {
			enum {
				ESI_Cube = 0,
				EC_Consts,
				Count
			};

			namespace RootConstant {
				enum {
					E_MipLevel = 0,
					Count
				};
			}
		}
	}

	namespace PipelineState {
		enum Type {
			E_ConvEquirectToCube = 0,
			E_ConvCubeToEquirect,
			Count
		};
	}

	class EquirectangularConverterClass {
	public:
		EquirectangularConverterClass() = default;
		virtual ~EquirectangularConverterClass() = default;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		void ConvertEquirectangularToCube(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* resource,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
			D3D12_CPU_DESCRIPTOR_HANDLE* ro_outputs);
		void ConvertEquirectangularToCube(
			ID3D12GraphicsCommandList* const cmdList,
			UINT width, UINT height,
			GpuResource* resource,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
			CD3DX12_CPU_DESCRIPTOR_HANDLE ro_outputs[][CubeMapFace::Count],
			UINT maxMipLevel);
		void ConvertCubeToEquirectangular(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* equirectResource,
			D3D12_CPU_DESCRIPTOR_HANDLE equirectRtv,
			D3D12_GPU_DESCRIPTOR_HANDLE cubeSrv,
			UINT mipLevel = 0);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignature::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	};
};