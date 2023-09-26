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
				EC_Consts,
				ESI_Cube,
				ESI_Equirectangular,
				Count
			};
		}

		namespace RootConstantsLayout {
			enum {
				E_MipLevel = 0,
				Count
			};
		}

		enum Type {
			E_Equirectangular = 0,
			E_EnvironmentCube,
			E_DiffuseIrradianceCube,
			E_SpecularIrradianceCube,
			Count
		};
	}

	namespace ConvoluteDiffuseIrradiance {
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

	namespace ConvoluteSpecularIrradiance {
		namespace RootSignatureLayout {
			enum {
				ECB_ConvEquirectToConv = 0,
				EC_Consts,
				ESI_Environment,
				Count
			};
		}

		namespace RootConstantsLayout {
			enum {
				E_FaceID = 0,
				E_Roughness,
				Count
			};
		}
	}

	namespace IntegrateBrdf {
		namespace RootSignatureLayout {
			enum {
				ECB_Pass = 0,
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
			E_ConvoluteDiffuseIrradiance,
			E_ConvoluteSpecularIrradiance,
			E_IntegrateBrdf,
			E_DrawCube,
			E_DrawEquirectangular,
			E_DrawSkySphere,
			Count
		};
	}

	namespace RootSignature {
		enum Type {
			E_ConvEquirectToCube = 0,
			E_ConvCubeToEquirect,
			E_ConvoluteDiffuseIrradiance,
			E_ConvoluteSpecularIrradiance,
			E_IntegrateBrdf,
			E_DrawCube,
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

	static const UINT MaxMipLevel = 5;
	// Environemt CubeMap(6) + Diffuse Irradiance CubeMap(6) + Diffuse Irradiance Equirectangular Map(1) 
	//	+ Specular Irradiance CubeMap(6 * 5) + Integrated BRDF Map(1)
	static const UINT NumRenderTargets = 
		CubeMapFace::Count + CubeMapFace::Count + 1 + (CubeMapFace::Count * MaxMipLevel) + 1;

	static const DXGI_FORMAT IntegratedBrdfMapFormat = DXGI_FORMAT_R16G16_FLOAT;

	class IrradianceMapClass {
	public:
		IrradianceMapClass();
		virtual ~IrradianceMapClass() = default;

	public:
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE EnvironmentCubeMapSrv() const;

		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE DiffuseIrradianceCubeMapSrv() const;
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE PrefilteredSpecularCubeMapSrv() const;
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE IntegratedBrdfMapSrv() const;

		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE DiffuseIrradianceEquirectMapSrv() const;

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
			D3D12_GPU_VIRTUAL_ADDRESS cbPass,
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
			RenderItem* cube,
			float mipLevel = 0.0f);
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
		bool Load(
			ID3D12CommandQueue* const queue,
			GpuResource* const dst,
			D3D12_CPU_DESCRIPTOR_HANDLE hDesc,
			LPCWSTR filename,
			LPCWSTR setname);
		bool Save(ID3D12CommandQueue* const queue, GpuResource* resource, LPCWSTR filepath);

		void ConvertEquirectangularToCube(
			ID3D12GraphicsCommandList* const cmdList,
			GpuResource* resource,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_equirectangular,
			D3D12_CPU_DESCRIPTOR_HANDLE* ro_outputs,
			RenderItem* box);
		void ConvertCubeToEquirectangular(ID3D12GraphicsCommandList* const cmdList);
		void ConvoluteDiffuse(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			RenderItem* box);
		void ConvoluteSpecular(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			RenderItem* box);
		void IntegrateBrdf(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbPass);

	public:
		DrawCube::Type DrawCubeType;

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
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEnvironmentCubeMapCpuRtvs[CubeMapFace::Count];

		std::unique_ptr<GpuResource> mDiffuseIrradianceCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceCubeMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceCubeMapCpuRtvs[CubeMapFace::Count];

		std::unique_ptr<GpuResource> mDiffuseIrradianceEquirectMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceEquirectMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceEquirectMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceEquirectMapCpuRtv;

		std::unique_ptr<GpuResource> mSpecularIrradianceCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularIrradianceCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhSpecularIrradianceCubeMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhSpecularIrradianceCubeMapCpuRtvs[MaxMipLevel][CubeMapFace::Count];

		std::unique_ptr<GpuResource> mIntegratedBrdfMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntegratedBrdfMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhIntegratedBrdfMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntegratedBrdfMapCpuRtv;

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

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::DiffuseIrradianceCubeMapSrv() const {
	return mhDiffuseIrradianceCubeMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::PrefilteredSpecularCubeMapSrv() const {
	return mhSpecularIrradianceCubeMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::IntegratedBrdfMapSrv() const {
	return mhIntegratedBrdfMapGpuSrv;
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE IrradianceMap::IrradianceMapClass::DiffuseIrradianceEquirectMapSrv() const {
	return mhDiffuseIrradianceEquirectMapGpuSrv;
}