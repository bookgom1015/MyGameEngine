#pragma once

#include <wrl.h>
#include <d3dx12.h>
#include <unordered_map>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace SVGF {
	namespace RootSignature {
		enum Type {
			E_TemporalSupersamplingReverseReproject = 0,
			E_TemporalSupersamplingBlendWithCurrentFrame,
			E_CalcDepthPartialDerivative,
			E_CalcLocalMeanVariance,
			E_FillInCheckerboard,
			E_AtrousWaveletTransformFilter,
			E_DisocclusionBlur,
			Count
		};

		namespace CalcAmbientOcclusion {
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

		namespace TemporalSupersamplingReverseReproject {
			enum {
				ECB_CrossBilateralFilter = 0,
				EC_Consts,
				ESI_NormalDepth,
				ESI_DepthPartialDerivative,
				ESI_ReprojectedNormalDepth,
				ESI_CachedNormalDepth,
				ESI_Velocity,
				ESI_CachedAOCoefficient,
				ESI_CachedTspp,
				ESI_CachedAOCoefficientSquaredMean,
				ESI_CachedRayHitDistance,
				EUO_CachedTspp,
				EUO_CachedValue,
				EUO_CachedSquaredMean,
				EUO_TsppSquaredMeanRayHitDistacne,
				Count
			};

			namespace RootConstant {
				enum {
					E_TextureDim_X = 0,
					E_TextureDim_Y,
					E_InvTextureDim_X,
					E_InvTextureDim_Y,
					Count
				};
			}
		}

		namespace TemporalSupersamplingBlendWithCurrentFrame {
			enum {
				ECB_TsspBlendWithCurrentFrame = 0,
				ESI_AOCoefficient,
				ESI_LocalMeanVaraince,
				ESI_RayHitDistance,
				ESI_CachedValue,
				ESI_CachedSquaredMean,
				ESI_TsppSquaredMeanRayHitDistance,
				EUIO_TemporalAOCoefficient,
				EUIO_Tspp,
				EUIO_CoefficientSquaredMean,
				EUIO_RayHitDistance,
				EUO_VarianceMap,
				EUO_BlurStrength,
				Count
			};
		}

		namespace CalcDepthPartialDerivative {
			enum {
				EC_Consts = 0,
				ESI_Depth,
				EUO_DepthPartialDerivative,
				Count
			};

			namespace RootConstant {
				enum {
					E_InvTextureDim_X = 0,
					E_InvTextureDim_Y,
					Count
				};
			}
		}

		namespace CalcLocalMeanVariance {
			enum {
				ECB_LocalMeanVar = 0,
				ESI_AOCoefficient,
				EUO_LocalMeanVar,
				Count
			};
		}

		namespace FillInCheckerboard {
			enum {
				ECB_LocalMeanVar = 0,
				EUIO_LocalMeanVar,
				Count
			};
		}

		namespace AtrousWaveletTransformFilter {
			enum {
				ECB_AtrousFilter = 0,
				EC_Consts,
				ESI_TemporalAOCoefficient,
				ESI_NormalDepth,
				ESI_Variance,
				ESI_HitDistance,
				ESI_DepthPartialDerivative,
				ESI_Tspp,
				EUO_TemporalAOCoefficient,
				Count
			};

			namespace RootConstant {
				enum {
					E_RayHitDistToKernelWidthScale = 0,
					E_RayHitDistToKernelSizeScaleExp,
					Count
				};
			}
		}

		namespace DisocclusionBlur {
			enum {
				EC_Consts = 0,
				ESI_Depth,
				ESI_RMS,
				ESI_BlurStrength,
				EUIO_AOCoefficient,
				Count
			};

			namespace RootConstant {
				enum {
					E_TextureDim_X = 0,
					E_TextureDim_Y,
					E_Step,
					E_MaxStep,
					Count
				};
			}
		}
	}

	namespace PipelineState {
		enum Type {
			E_TemporalSupersamplingReverseReproject_Contrast = 0,
			E_TemporalSupersamplingReverseReproject_HDR,
			E_TemporalSupersamplingBlendWithCurrentFrame_Contrast,
			E_TemporalSupersamplingBlendWithCurrentFrame_HDR,
			E_CalcDepthPartialDerivative,
			E_CalcLocalMeanVariance_Contrast,
			E_CalcLocalMeanVariance_HDR,
			E_FillInCheckerboard,
			E_AtrousWaveletTransformFilter_Contrast,
			E_AtrousWaveletTransformFilter_HDR,
			E_DisocclusionBlur_Contrast,
			E_DisocclusionBlur_HDR,
			Count
		};
	}

	namespace Resource {
		namespace LocalMeanVariance {
			enum {
				E_Raw = 0,
				E_Smoothed,
				Count
			};
		}

		namespace Variance {
			enum {
				E_Raw = 0,
				E_Smoothed,
				Count
			};
		}

		namespace CachedValue {
			enum {
				E_Contrast = 0,
				E_Color_HDR,
				Count
			};
		}

		namespace CachedSquaredMean {
			enum {
				E_Contrast = 0,
				E_Color_HDR,
				Count
			};
		}
	}

	namespace Descriptor {
		namespace LocalMeanVariance {
			enum {
				ES_Raw = 0,
				EU_Raw,
				ES_Smoothed,
				EU_Smoothed,
				Count
			};
		}

		namespace Variance {
			enum {
				ES_Raw = 0,
				EU_Raw,
				ES_Smoothed,
				EU_Smoothed,
				Count
			};
		}

		namespace CachedValue {
			enum {
				ES_Contrast = 0,
				EU_Contrast,
				ES_Color_HDR,
				EU_Color_HDR,
				Count
			};
		}

		namespace CachedSquaredMean {
			enum {
				ES_Contrast = 0,
				EU_Contrast,
				ES_Color_HDR,
				EU_Color_HDR,
				Count
			};
		}
	}

	namespace Value {
		enum Type {
			E_Contrast = 0,
			E_Color_HDR,
			Count
		};
	}

	using LocalMeanVarianceResourcesType = std::array<std::unique_ptr<GpuResource>, Resource::LocalMeanVariance::Count>;
	using LocalMeanVarianceResourcesCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::LocalMeanVariance::Count>;
	using LocalMeanVarianceResourcesGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::LocalMeanVariance::Count>;

	using VarianceResourcesType = std::array<std::unique_ptr<GpuResource>, Resource::Variance::Count>;
	using VarianceResourcesCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::Variance::Count>;
	using VarianceResourcesGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::Variance::Count>;

	using CachedValueResourcesType = std::array<std::unique_ptr<GpuResource>, Resource::CachedValue::Count>;
	using CachedValueResourcesCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::CachedValue::Count>;
	using CachedValueResourcesGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::CachedValue::Count>;

	using CachedSquaredMeanResourcesType = std::array<std::unique_ptr<GpuResource>, Resource::CachedSquaredMean::Count>;
	using CachedSquaredMeanResourcesCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::CachedSquaredMean::Count>;
	using CachedSquaredMeanResourcesGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::CachedSquaredMean::Count>;

	class SVGFClass {
	public:
		SVGFClass();
		virtual ~SVGFClass() = default;

	public:
		__forceinline const LocalMeanVarianceResourcesType& LocalMeanVarianceResources() const;
		__forceinline const LocalMeanVarianceResourcesGpuDescriptors& LocalMeanVarianceResourcesGpuDescriptors() const;

		__forceinline const VarianceResourcesType& VarianceResources() const;
		__forceinline const VarianceResourcesGpuDescriptors& VarianceResourcesGpuDescriptors() const;

		__forceinline GpuResource* DepthPartialDerivativeMapResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthPartialDerivativeSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthPartialDerivativeUav() const;

		__forceinline GpuResource* DisocclusionBlurStrengthResource();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DisocclusionBlurStrengthSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DisocclusionBlurStrengthUav() const;

		__forceinline GpuResource* TsppSquaredMeanRayHitDistance();
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE TsppSquaredMeanRayHitDistanceSrv() const;
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE TsppSquaredMeanRayHitDistanceUav() const;

	public:
		BOOL Initialize(ID3D12Device5* const device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignatures(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);
		BOOL OnResize(UINT width, UINT height);
		
		void RunCalculatingDepthPartialDerivative(
			ID3D12GraphicsCommandList4* const cmdList,
			D3D12_GPU_DESCRIPTOR_HANDLE i_depth,
			UINT width, UINT height);
		void RunCalculatingLocalMeanVariance(
			ID3D12GraphicsCommandList4* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_value,
			UINT width, UINT height,
			BOOL checkerboardSamplingEnabled);
		void FillInCheckerboard(
			ID3D12GraphicsCommandList4* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			UINT width, UINT height);
		void ReverseReprojectPreviousFrame(
			ID3D12GraphicsCommandList4* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_reprojNormalDepth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_cachedNormalDepth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
			D3D12_GPU_DESCRIPTOR_HANDLE si_cachedValue,
			D3D12_GPU_DESCRIPTOR_HANDLE si_cachedTspp,
			D3D12_GPU_DESCRIPTOR_HANDLE si_cachedValueSquaredMean,
			D3D12_GPU_DESCRIPTOR_HANDLE si_cachedRayHitDistance,
			D3D12_GPU_DESCRIPTOR_HANDLE uo_cachedTspp,
			UINT width, UINT height,
			Value::Type type);
		void BlendWithCurrentFrame(
			ID3D12GraphicsCommandList4* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_value,
			D3D12_GPU_DESCRIPTOR_HANDLE si_rayHitDistance,
			D3D12_GPU_DESCRIPTOR_HANDLE uio_temporalValue,
			D3D12_GPU_DESCRIPTOR_HANDLE uio_tspp,
			D3D12_GPU_DESCRIPTOR_HANDLE uio_valueSquaredMean,
			D3D12_GPU_DESCRIPTOR_HANDLE uio_rayHitDistance,
			UINT width, UINT height,
			Value::Type type);
		void ApplyAtrousWaveletTransformFilter(
			ID3D12GraphicsCommandList4* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_temporalValue,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_hitDistance,
			D3D12_GPU_DESCRIPTOR_HANDLE si_tspp,
			D3D12_GPU_DESCRIPTOR_HANDLE uo_temporalValue,
			UINT width, UINT height,
			FLOAT rayHitDistToKernelWidthScale,
			FLOAT rayHitDistToKernelSizeScaleExp,
			Value::Type type,
			bool useSmoothingVar);
		void BlurDisocclusion(
			ID3D12GraphicsCommandList4* const cmdList,
			GpuResource* value,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
			D3D12_GPU_DESCRIPTOR_HANDLE uio_value,
			UINT width, UINT height,
			UINT lowTsppBlurPasses,
			Value::Type type);

	private:
		void BuildDescriptors();
		BOOL BuildResources(UINT width, UINT height);

	private:
		ID3D12Device5* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignature::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPsos;

		SVGF::LocalMeanVarianceResourcesType mLocalMeanVarianceResources;
		SVGF::LocalMeanVarianceResourcesCpuDescriptors mhLocalMeanVarianceResourcesCpus;
		SVGF::LocalMeanVarianceResourcesGpuDescriptors mhLocalMeanVarianceResourcesGpus;

		SVGF::VarianceResourcesType mVarianceResources;
		SVGF::VarianceResourcesCpuDescriptors mhVarianceResourcesCpus;
		SVGF::VarianceResourcesGpuDescriptors mhVarianceResourcesGpus;
		
		SVGF::CachedValueResourcesType mCachedValues;
		SVGF::CachedValueResourcesCpuDescriptors mhCachedValueCpus;
		SVGF::CachedValueResourcesGpuDescriptors mhCachedValueGpus;

		CachedSquaredMeanResourcesType mCachedSquaredMeans;
		CachedSquaredMeanResourcesCpuDescriptors mhCachedSquaredMeanCpus;
		CachedSquaredMeanResourcesGpuDescriptors mhCachedSquaredMeanGpus;

		std::unique_ptr<GpuResource> mDepthPartialDerivative;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthPartialDerivativeCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthPartialDerivativeGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthPartialDerivativeCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthPartialDerivativeGpuUav;

		std::unique_ptr<GpuResource> mDisocclusionBlurStrength;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDisocclusionBlurStrengthCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDisocclusionBlurStrengthGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDisocclusionBlurStrengthCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhDisocclusionBlurStrengthGpuUav;

		std::unique_ptr<GpuResource> mTsppSquaredMeanRayHitDistance;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhTsppSquaredMeanRayHitDistanceCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhTsppSquaredMeanRayHitDistanceGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhTsppSquaredMeanRayHitDistanceCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhTsppSquaredMeanRayHitDistanceGpuUav;

		UINT mTemporalCurrentFrameResourceIndex = 0;
		UINT mTemporalCurrentFrameTemporalCeofficientResourceIndex = 0;
	};
}

#include "SVGF.inl"