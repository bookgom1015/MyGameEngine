#pragma once

#include <DirectX/d3dx12.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>

#include "Common/Util/Locker.h"
#include "DirectX/Util/MipmapGenerator.h"

#include "Samplers.h"
#include "HlslCompaction.h"	

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace EquirectangularConverter { class EquirectangularConverterClass; }

namespace IrradianceMap {
	namespace RootSignature {
		enum {
			E_ConvoluteDiffuseIrradiance = 0,
			E_ConvolutePrefilteredIrradiance,
			E_IntegrateBRDF,
			E_DrawSkySphere,
			Count
		};

		namespace ConvoluteDiffuseIrradiance {
			enum {
				ECB_ConvEquirectToConv = 0,
				ESI_Cube,
				Count
			};
		}

		namespace ConvoluteSpecularIrradiance {
			enum {
				ECB_ConvEquirectToConv = 0,
				EC_Consts,
				ESI_Environment,
				Count
			};
		}

		namespace IntegrateBRDF {
			enum {
				ECB_Pass = 0,
				Count
			};
		}

		namespace DrawSkySphere {
			enum {
				ECB_Pass = 0,
				ECB_Obj,
				ESI_Cube,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
			EG_ConvEquirectToCube = 0,
			EG_ConvCubeToEquirect,
			EG_ConvoluteDiffuseIrradiance,
			EG_ConvolutePrefilteredIrradiance,
			EG_IntegrateBRDF,
			EG_DrawSkySphere,
			Count
		};
	}

	namespace Save {
		enum Type : std::uint8_t {
			E_None				= 1 << 0,
			E_DiffuseIrradiance	= 1 << 1,
			E_IntegratedBRDF	= 1 << 2,
			E_PrefilteredL0		= 1 << 3,
			E_PrefilteredL1		= 1 << 4,
			E_PrefilteredL2		= 1 << 5,
			E_PrefilteredL3		= 1 << 6,
			E_PrefilteredL4		= 1 << 7,
		};
	}

	static const UINT MaxMipLevel = 5;

	static const UINT NumRenderTargets = 
		MaxMipLevel		// Equirectangular Map(5)
		+ MaxMipLevel	// Environemt CubeMap(5) 
		+ 1				// Diffuse Irradiance CubeMap(1)
		+ 1				// Diffuse Irradiance Equirectangular Map(1)
		+ MaxMipLevel	// Prefiltered Irradiance CubeMap(5)
		+ 1				// Integrated BRDF Map(1)
		+ MaxMipLevel;	// Prefiltered Irradiance Equirectangular Map(5)

	class IrradianceMapClass {
	public:
		IrradianceMapClass();
		virtual ~IrradianceMapClass() = default;

	public:
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE EquirectangularMapSrv() const;
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE TemporaryEquirectangularMapSrv() const;

		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE EnvironmentCubeMapSrv() const;

		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE DiffuseIrradianceCubeMapSrv() const;
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE PrefilteredEnvironmentCubeMapSrv() const;
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE IntegratedBrdfMapSrv() const;

		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE DiffuseIrradianceEquirectMapSrv() const;

		UINT Size() const;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();

		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);
		BOOL BuildDescriptors();

		BOOL SetEquirectangularMap(ID3D12CommandQueue* const queue, const std::string& file);

		BOOL Update(
			ID3D12Device5* const device,
			ID3D12CommandQueue* const queue,
			ID3D12DescriptorHeap* const descHeap,
			ID3D12GraphicsCommandList* const cmdList,
			EquirectangularConverter::EquirectangularConverterClass* const converter,
			D3D12_GPU_VIRTUAL_ADDRESS cbPass,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			MipmapGenerator::MipmapGeneratorClass* const generator);
		BOOL DrawSkySphere(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
			D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize,
			RenderItem* const sphere);

	private:
		BOOL BuildResources();

		BOOL Check(const std::wstring& filepath);
		BOOL Load(
			ID3D12Device5* const device,
			ID3D12CommandQueue* const queue,
			GpuResource* const dst,
			D3D12_CPU_DESCRIPTOR_HANDLE hDesc,
			const std::wstring& filepath,
			LPCWSTR setname);
		BOOL Save(ID3D12CommandQueue* const queue, GpuResource* resource, const std::wstring& filepath);

		void GenerateDiffuseIrradiance(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube);
		void GeneratePrefilteredEnvironment(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube);
		void GenerateIntegratedBrdf(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbPass);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, RootSignature::Count> mRootSignatures;
		std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineState::Count> mPSOs;

		std::unique_ptr<GpuResource> mTemporaryEquirectangularMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhTemporaryEquirectangularMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhTemporaryEquirectangularMapGpuSrv;

		std::unique_ptr<GpuResource> mEquirectangularMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEquirectangularMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhEquirectangularMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEquirectangularMapCpuRtvs[MaxMipLevel];

		std::unique_ptr<GpuResource> mEnvironmentCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEnvironmentCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhEnvironmentCubeMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEnvironmentCubeMapCpuRtvs[MaxMipLevel];

		std::unique_ptr<GpuResource> mDiffuseIrradianceCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceCubeMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceCubeMapCpuRtv;

		std::unique_ptr<GpuResource> mDiffuseIrradianceEquirectMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceEquirectMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceEquirectMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseIrradianceEquirectMapCpuRtv;

		std::unique_ptr<GpuResource> mPrefilteredEnvironmentCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhPrefilteredEnvironmentCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhPrefilteredEnvironmentCubeMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhPrefilteredEnvironmentCubeMapCpuRtvs[MaxMipLevel];

		std::unique_ptr<GpuResource> mPrefilteredEnvironmentEquirectMaps[MaxMipLevel];
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhPrefilteredEnvironmentEquirectMapCpuSrvs[MaxMipLevel];
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhPrefilteredEnvironmentEquirectMapGpuSrvs[MaxMipLevel];
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhPrefilteredEnvironmentEquirectMapCpuRtvs[MaxMipLevel];

		std::unique_ptr<GpuResource> mIntegratedBrdfMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntegratedBrdfMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhIntegratedBrdfMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntegratedBrdfMapCpuRtv;

		D3D12_VIEWPORT mCubeMapViewport;
		D3D12_RECT mCubeMapScissorRect;

		D3D12_VIEWPORT mIrradEquirectMapViewport;
		D3D12_RECT mIrradEquirectMapScissorRect;

		BOOL bNeedToUpdate = FALSE;
		Save::Type mNeedToSave = Save::E_None;
	};
}

#include "IrradianceMap.inl"