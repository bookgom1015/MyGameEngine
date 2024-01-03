#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <array>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;
class GpuResource;

namespace Rtao {
	namespace RootSignature {
		enum {
			ESI_AccelerationStructure = 0,
			ECB_RtaoPass,
			ESI_Pos,
			ESI_Normal,
			ESI_Depth,
			EUO_AOCoefficient,
			EUO_RayHitDistance,
			Count
		};
	}

	namespace Resource {
		namespace AO {
			enum {
				E_AmbientCoefficient = 0,
				E_RayHitDistance,
				Count
			};
		}

		namespace TemporalCache {
			enum {
				E_Tspp = 0,
				E_RayHitDistance,
				E_CoefficientSquaredMean,
				Count
			};
		}
	}

	namespace Descriptor {
		namespace AO {
			enum {
				ES_AmbientCoefficient = 0,
				EU_AmbientCoefficient,
				ES_RayHitDistance,
				EU_RayHitDistance,
				Count
			};
		}

		namespace TemporalCache {
			enum {
				ES_Tspp = 0,
				EU_Tspp,
				ES_RayHitDistance,
				EU_RayHitDistance,
				ES_CoefficientSquaredMean,
				EU_CoefficientSquaredMean,
				Count
			};
		}

		namespace TemporalAOCoefficient {
			enum {
				Srv = 0,
				Uav,
				Count
			};
		}
	}

	using AOResourcesType = std::array<std::unique_ptr<GpuResource>, Resource::AO::Count>;
	using AOResourcesCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::AO::Count>;
	using AOResourcesGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::AO::Count>;

	using TemporalCachesType = std::array<std::array<std::unique_ptr<GpuResource>, Resource::TemporalCache::Count>, 2>;
	using TemporalCachesCpuDescriptors = std::array<std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::TemporalCache::Count>, 2>;
	using TemporalCachesGpuDescriptors = std::array<std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::TemporalCache::Count>, 2>;

	using TemporalAOCoefficientsType = std::array<std::unique_ptr<GpuResource>, 2>;
	using TemporalAOCoefficientsCpuDescriptors = std::array<std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::TemporalAOCoefficient::Count>, 2>;
	using TemporalAOCoefficientsGpuDescriptors = std::array<std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::TemporalAOCoefficient::Count>, 2>;

	const FLOAT AmbientMapClearValues[1] = { 1.0f };

	class RtaoClass {
	public:
		RtaoClass();
		virtual ~RtaoClass() = default;

	public:
		__forceinline const AOResourcesType& AOResources() const;
		__forceinline const AOResourcesGpuDescriptors& AOResourceGpuDescriptors() const;

		__forceinline const TemporalCachesType& TemporalCaches() const;
		__forceinline const TemporalCachesGpuDescriptors& TemporalCacheGpuDescriptors() const;

		__forceinline const TemporalAOCoefficientsType& TemporalAOCoefficients();
		__forceinline const TemporalAOCoefficientsGpuDescriptors& TemporalAOCoefficientGpuDescriptors() const;

		__forceinline constexpr UINT TemporalCurrentFrameResourceIndex() const;
		__forceinline constexpr UINT TemporalCurrentFrameTemporalAOCoefficientResourceIndex() const;

		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ResolvedAOCoefficientSrv() const;

	public:
		BOOL Initialize(ID3D12Device5* const device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignatures(const StaticSamplers& samplers);
		BOOL BuildPSO();
		BOOL BuildShaderTables(UINT numRitems);
		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);
		BOOL OnResize(UINT width, UINT height);

		void RunCalculatingAmbientOcclusion(
			ID3D12GraphicsCommandList4* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			UINT width, UINT height);

		UINT MoveToNextFrame();
		UINT MoveToNextFrameTemporalAOCoefficient();

	private:
		void BuildDescriptors();
		BOOL BuildResources(UINT width, UINT height);

	private:
		ID3D12Device5* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12StateObject> mDxrPso;
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mDxrPsoProp;

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> mShaderTables;

		Rtao::AOResourcesType mAOResources;
		Rtao::AOResourcesCpuDescriptors mhAOResourcesCpus;
		Rtao::AOResourcesGpuDescriptors mhAOResourcesGpus;

		Rtao::TemporalCachesType mTemporalCaches;
		Rtao::TemporalCachesCpuDescriptors mhTemporalCachesCpus;
		Rtao::TemporalCachesGpuDescriptors mhTemporalCachesGpus;

		Rtao::TemporalAOCoefficientsType mTemporalAOCoefficients;
		Rtao::TemporalAOCoefficientsCpuDescriptors mhTemporalAOCoefficientsCpus;
		Rtao::TemporalAOCoefficientsGpuDescriptors mhTemporalAOCoefficientsGpus;

		UINT mTemporalCurrentFrameResourceIndex = 0;
		UINT mTemporalCurrentFrameTemporalAOCeofficientResourceIndex = 0;
	};
}

#include "Rtao.inl"