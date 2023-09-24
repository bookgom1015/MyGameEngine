#pragma once

#include <d3dx12.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace IrradianceMap {
	namespace ConvEquirectToCube {
		namespace RootSignatureLayout {
			enum {
				ECB_ConvEquirectToCube = 0,
				EC_Consts,
				ESI_Equirectangular,
				Count
			};
		}

		namespace RootConstantsLayout {
			enum {
				E_FaceID = 0,
				Count
			};
		}
	}

	namespace ConvCubeToEquirect {
		namespace RootSignatureLayout {
			enum {
				ESI_Cube = 0,
				Count
			};
		}
	}

	namespace DrawCube {
		namespace RootSignatureLayout {
			enum {
				ECB_Pass = 0,
				ECB_Obj,
				ESI_Cube,
				ESI_Equirectangular,
				Count
			};
		}
	}

	namespace ConvoluteIrradiance {
		namespace RootSignatureLayout {
			enum {
				ECB_ConvEquirectToConv = 0,
				EC_Consts,
				ESI_Cube,
				Count
			};
		}
		
		namespace RootConstantsLayout {
			enum {
				E_FaceID = 0,
				E_SampDelta,
				Count
			};
		}
	}

	namespace DrawSkySphere {
		namespace RootSignatureLayout {
			enum {
				ECB_Pass = 0,
				ECB_Obj,
				ESI_Cube,
				Count
			};
		}
	}

	namespace PipelineState {
		enum Type {
			E_ConvEquirectToCube = 0,
			E_ConvCubeToEquirect,
			E_DrawEquirectangularCube,
			E_DrawConvertedCube,
			E_DrawIrradianceCube,
			E_ConvoluteIrradiance,
			E_DrawSkySphere,
			Count
		};
	}

	namespace RootSignature {
		enum Type {
			E_ConvEquirectToCube = 0,
			E_ConvCubeToEquirect,
			E_DrawCube,
			E_ConvoluteIrradiance,
			E_DrawSkySphere,
			Count
		};
	}

	namespace CubeMapFace {
		enum Type {
			E_PositiveX = 0,	// Right
			E_NegativeX,		// Left
			E_PositiveY,		// Upper
			E_NegativeY,		// Bottom
			E_PositiveZ,		// Frontward
			E_NegativeZ,		// Backward
			Count
		};
	}

	static const UINT NumRenderTargets = 6 + 6 + 1;

	class IrradianceMapClass {
	public:
		IrradianceMapClass();
		virtual ~IrradianceMapClass() = default;

	public:
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE EnvironmentCubeMapSrv() const;
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceCubeMapSrv() const;
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceEquirectMapSrv() const;

	public:
		bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList,ShaderManager* const manager);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);

		bool SetEquirectangularMap(ID3D12CommandQueue* const queue,  const std::string& file);

		bool Update(
			ID3D12CommandQueue*const queue,
			ID3D12DescriptorHeap* descHeap,
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			RenderItem* box);
		bool DrawCubeMap(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize,
			RenderItem* cube);
		bool DrawSkySphere(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
			D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize,
			RenderItem* sphere);

	private:
		void BuildDescriptors();
		bool BuildResources(ID3D12GraphicsCommandList* const cmdList);

		bool Check();
		bool Load(ID3D12CommandQueue* const queue, const std::wstring& filename);
		bool Save(ID3D12CommandQueue*const queue);

		void Convert(
			ID3D12GraphicsCommandList* const cmdList,
			GpuResource* resource,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
			D3D12_CPU_DESCRIPTOR_HANDLE* ro_outputs,
			RenderItem* box);
		void Convert(ID3D12GraphicsCommandList* const cmdList);
		void Convolute(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			RenderItem* box);

	public:
		PipelineState::Type PipelineStateType;

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignature::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		std::unique_ptr<GpuResource> mEquirectangularMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEquirectangularMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhEquirectangularMapGpuSrv;

		std::unique_ptr<GpuResource> mEnvironmentCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEnvironmentCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhEnvironmentCubeMapGpuSrv;
		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mhEnvironmentCubeMapCpuRtvs;

		std::unique_ptr<GpuResource> mIrradianceCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIrradianceCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhIrradianceCubeMapGpuSrv;
		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mhIrradianceCubeMapCpuRtvs;

		std::unique_ptr<GpuResource> mIrradianceEquirectMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIrradianceEquirectMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhIrradianceEquirectMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIrradianceEquirectMapCpuRtv;

		D3D12_VIEWPORT mCubeMapViewport;
		D3D12_RECT mCubeMapScissorRect;

		D3D12_VIEWPORT mIrradEquirectMapViewport;
		D3D12_RECT mIrradEquirectMapScissorRect;

		bool bNeedToUpdate;
		bool bNeedToSave;
	};
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::EnvironmentCubeMapSrv() const {
	return mhEnvironmentCubeMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::IrradianceCubeMapSrv() const {
	return mhIrradianceCubeMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::IrradianceEquirectMapSrv() const {
	return mhIrradianceEquirectMapGpuSrv;
}