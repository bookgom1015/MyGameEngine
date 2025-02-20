#pragma once

#ifdef HLSL
	#ifndef HDR_FORMAT
		#define HDR_FORMAT float4
	#endif
	
	#ifndef SDR_FORMAT
		#define SDR_FORMAT float4
	#endif

	#ifndef COMPACT_NORMAL_DEPTH_DXGI_FORMAT
		#define COMPACT_NORMAL_DEPTH_DXGI_FORMAT uint
	#endif

	#ifndef CONTRAST_VALUE_FORMAT
		#define CONTRAST_VALUE_FORMAT float
	#endif

	#ifndef HDR_COLOR_VALUE_FORMAT
		#define HDR_COLOR_VALUE_FORMAT HDR_FORMAT
	#endif
#else
	#ifndef HDR_FORMAT
		#define HDR_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
	#endif
	
	#ifndef SDR_FORMAT
		#define SDR_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM
	#endif
	
	#ifndef COMPACT_NORMAL_DEPTH_DXGI_FORMAT
		#define COMPACT_NORMAL_DEPTH_DXGI_FORMAT DXGI_FORMAT_R32_UINT
	#endif

	#ifndef CONTRAST_VALUE_FORMAT
		#define CONTRAST_VALUE_FORMAT DXGI_FORMAT_R16_FLOAT
	#endif

	#ifndef HDR_COLOR_VALUE_FORMAT
		#define HDR_COLOR_VALUE_FORMAT HDR_FORMAT
	#endif
#endif

namespace GBuffer {
	static const FLOAT InvalidDepthValue = 1.f;
	static const FLOAT InvalidNormDepthValue = 0.f;
	static const FLOAT InvalidVelocityValue = 1000.f;	

#ifndef GBuffer_Default_RCSTRUCT
	#define GBuffer_Default_RCSTRUCT {	\
		FLOAT gMaxDistance;				\
		FLOAT gMinDistance;				\
	};
#endif

#ifdef HLSL
	typedef float4								AlbedoMapFormat;
	typedef float4								NormalMapFormat;
	typedef COMPACT_NORMAL_DEPTH_DXGI_FORMAT	NormalDepthMapFormat;
	typedef float								DepthMapFormat;
	typedef float4								RMSMapFormat;
	typedef float2								VelocityMapFormat;
	typedef uint								ReprojNormalDepthMapFormat;
	typedef float4								PositionMapFormat;

	static const float InvalidPositionValueW	= -1;
	static const float InvalidNormalValueW		= -1;

	bool IsInvalidDepth(float val);
	bool IsValidDepth(float val);

	bool IsInvalidVelocity(float2 val);
	bool IsValidVelocity(float2 val);

	bool IsInvalidPosition(float4 val);
	bool IsValidPosition(float4 val);

	#ifndef GBuffer_Default_RootConstants
		#define GBuffer_Default_RootConstants(reg) cbuffer cbRootConstant : register(reg) GBuffer_Default_RCSTRUCT
	#endif
#else 
	static const DXGI_FORMAT AlbedoMapFormat			= DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT NormalMapFormat			= DXGI_FORMAT_R16G16B16A16_FLOAT;
	static const DXGI_FORMAT NormalDepthMapFormat		= COMPACT_NORMAL_DEPTH_DXGI_FORMAT;
	static const DXGI_FORMAT DepthMapFormat				= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	static const DXGI_FORMAT RMSMapFormat				= DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT VelocityMapFormat			= DXGI_FORMAT_R16G16_FLOAT;
	static const DXGI_FORMAT PositionMapFormat			= DXGI_FORMAT_R16G16B16A16_FLOAT;

	const FLOAT AlbedoMapClearValues[4]					= { 0.f,  0.f, 0.f,  0.f };
	const FLOAT NormalMapClearValues[4]					= { 0.f,  0.f, 0.f, -1.f };
	const FLOAT NormalDepthMapClearValues[4]			= { 0.f,  0.f, 0.f,  0.f };
	const FLOAT RMSMapClearValues[4]					= { 0.5f, 0.f, 0.5f, 0.f };
	const FLOAT VelocityMapClearValues[2]				= { InvalidVelocityValue, InvalidVelocityValue };
	const FLOAT ReprojNormalDepthMapClearValues[4]		= { 0.f, 0.f, 0.f,  0.f };
	const FLOAT PositionMapClearValues[4]				= { 0.f, 0.f, 0.f, -1.f };

	namespace RootConstant {
		namespace Default {
			struct Struct GBuffer_Default_RCSTRUCT
			enum {
				EC_MaxDistance = 0,
				EC_MinDistance,
				Count
			};
		}
	}
#endif 
}

namespace Shadow {
#ifndef Shadow_ZDepth_RCSTRUCT
	#define Shadow_ZDepth_RCSTRUCT {	\
		UINT gLightIndex;				\
	};
#endif
#ifndef Shadow_Shadow_RCSTRUCT			
	#define Shadow_Shadow_RCSTRUCT {	\
		UINT gLightIndex;				\
	};
#endif

#ifdef HLSL
	typedef float	ZDepthMapFormat;
	typedef uint	ShadowMapFormat;
	typedef float4	DebugMapFormat;
	typedef float	VSDepthCubeMapFormat;
	typedef float	FaceIDCubeMapFormat;

	static const float	InvalidVSDepth	= -1.f;
	static const uint	InvalidFaceID	= 255;

	bool IsValidFaceID(uint faceID) {
		return faceID != InvalidFaceID;
	}

	bool IsInvalidFaceID(uint faceID) {
		return faceID == InvalidFaceID;
	}

	#ifndef Shadow_ZDepth_RootConstants
		#define Shadow_ZDepth_RootConstants(reg) cbuffer cbRootConstants : register(reg) Shadow_ZDepth_RCSTRUCT
	#endif
	#ifndef Shadow_Shadow_RootConstants
		#define Shadow_Shadow_RootConstants(reg) cbuffer cbRootConstants : register(reg) Shadow_Shadow_RCSTRUCT
	#endif
#else 
	static const DXGI_FORMAT ZDepthMapFormat		= DXGI_FORMAT_D32_FLOAT;
	static const DXGI_FORMAT ShadowMapFormat		= DXGI_FORMAT_R16_UINT;
	static const DXGI_FORMAT DebugMapFormat			= DXGI_FORMAT_R8G8B8A8_SNORM;
	static const DXGI_FORMAT VSDepthCubeMapFormat	= DXGI_FORMAT_R16_FLOAT;
	static const DXGI_FORMAT FaceIDCubeMapFormat	= DXGI_FORMAT_R16_FLOAT;

	const FLOAT VSDepthCubeMapFormatClearValues[4]	= { -1.f, 0.f, 0.f, 0.f };
	const FLOAT FaceIDCubeMapClearValues[4]			= { 255.f, 0.f, 0.f, 0.f };
#endif 
	namespace RootConstant {
		namespace ZDepth {
			struct Struct Shadow_ZDepth_RCSTRUCT
			enum {
				E_LightIndex = 0,
				Count
			};
		}

		namespace Shadow {
			struct Struct Shadow_Shadow_RCSTRUCT
			enum {
				E_LightIndex = 0,
				Count
			};
		}
	}

	namespace Default {
		namespace ThreadGroup {
			enum {
				Width = 8,
				Height = 8,
				Size = Width * Height
			};
		}
	}
}

namespace DXR_Shadow {
#ifdef HLSL
	typedef uint ShadowMapFormat;
#else 
	static const DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_R16_UINT;
#endif 
}

namespace DepthStencilBuffer {
#ifdef HLSL
	typedef float BufferFormat;
#else
	const DXGI_FORMAT BufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif

	static const FLOAT InvalidDepthValue = 1.f;
}

namespace TemporalAA {
#ifndef TemporalAA_Default_RCSTRUCT
	#define TemporalAA_Default_RCSTRUCT {	\
		FLOAT gModulationFactor;			\
	};
#endif

#ifdef HLSL
	#ifndef TemporalAA_Default_RootConstants
		#define TemporalAA_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) TemporalAA_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct TemporalAA_Default_RCSTRUCT

			enum {
				E_ModulationFactor = 0,
				Count
			};
		}
	}
#endif
}

namespace ToneMapping {
#ifndef ToneMapping_Default_RCSTRUCT
	#define ToneMapping_Default_RCSTRUCT {	\
		FLOAT gExposure;					\
	};
#endif

#ifdef HLSL
	typedef HDR_FORMAT IntermediateMapFormat;

	#ifndef ToneMapping_Default_RootConstants
		#define ToneMapping_Default_RootConstants(reg) cbuffer gRootConstants : register(reg) ToneMapping_Default_RCSTRUCT
	#endif
#else
	const DXGI_FORMAT IntermediateMapFormat = HDR_FORMAT;

	const FLOAT IntermediateMapClearValues[4] = { 0.f, 0.f, 0.f, 0.f };

	namespace RootConstant {
		namespace Default {
			struct Struct ToneMapping_Default_RCSTRUCT
			enum {
				E_Exposure = 0,
				Count
			};
		}
	}
#endif
}

namespace SSAO {
#ifdef HLSL
	typedef float	AOCoefficientMapFormat;
	typedef float3	RandomVectorMapFormat;
#else
	static const DXGI_FORMAT AOCoefficientMapFormat	= DXGI_FORMAT_R16_UNORM;
	static const DXGI_FORMAT RandomVectorMapFormat	= DXGI_FORMAT_R8G8B8A8_SNORM;
#endif
}

namespace SSR {
#ifdef HLSL
	typedef HDR_FORMAT SSRMapFormat;
#else
	static const DXGI_FORMAT SSRMapFormat = HDR_FORMAT;

	const FLOAT SSRMapClearValues[4] = { 0.f, 0.f, 0.f, 0.f };
#endif
}

namespace Bloom {
#ifndef Bloom_ExtractHighlight_RCSTRUCT
	#define Bloom_ExtractHighlight_RCSTRUCT {	\
		FLOAT gThreshold;							\
	};
#endif

#ifdef HLSL
	typedef HDR_FORMAT BloomMapFormat;

	#ifndef Bloom_ExtractHighlight_RootConstants
		#define Bloom_ExtractHighlight_RootConstants(reg) cbuffer cbRootConstants : register(reg) Bloom_ExtractHighlight_RCSTRUCT
	#endif
#else
	static const DXGI_FORMAT BloomMapFormat = HDR_FORMAT;

	namespace RootConstant {
		namespace ExtractHighlight {
			enum {
				EThreshold = 0,
				Count
			};
		}
	}
#endif
}

namespace MotionBlur {
#ifndef MotionBlur_Default_RCSTRUCT
	#define MotionBlur_Default_RCSTRUCT {	\
		FLOAT gIntensity;					\
		FLOAT gLimit;						\
		FLOAT gDepthBias;					\
		UINT  gSampleCount;					\
	};
#endif

#ifdef HLSL
	#ifndef MotionBlur_Default_RootConstants
		#define MotionBlur_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) MotionBlur_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct MotionBlur_Default_RCSTRUCT

			enum {
				E_Intensity = 0,
				E_Limit,
				E_DepthBias,
				E_SampleCount,
				Count
			};
		}
	}
#endif
}

namespace DepthOfField {
#ifndef DepthOfField_ApplyDoF_RCSTRUCT
	#define DepthOfField_ApplyDoF_RCSTRUCT {	\
		FLOAT gBokehRadius;						\
		FLOAT gMaxDevTolerance;					\
		FLOAT gHighlightPower;					\
		INT   gSampleCount;						\
	};
#endif

#ifdef HLSL
	typedef float	CoCMapFormat;
	typedef float	FocalDistanceBufferFormat;

	#ifndef DepthOfField_ApplyDoF_RootConstants
		#define DepthOfField_ApplyDoF_RootConstants(reg) cbuffer cbRootConstants : register(reg) DepthOfField_ApplyDoF_RCSTRUCT
	#endif
#else
	static const DXGI_FORMAT CoCMapFormat = DXGI_FORMAT_R16_FLOAT;

	namespace RootConstant {
		namespace ApplyDoF {
			struct Struct DepthOfField_ApplyDoF_RCSTRUCT
			enum {
				E_BokehRadius = 0,
				E_MaxDevTolerance,
				E_HighlightPower,
				E_SampleCount,
				Count
			};
		}
	}
#endif
}

namespace Sharpen {
#ifndef Sharpen_Default_RCSTRUCT
	#define Sharpen_Default_RCSTRUCT {		\
		DirectX::XMFLOAT2	gInvTexSize;	\
		FLOAT				gAmount;		\
	};
#endif

#ifdef HLSL
	#ifndef Sharpen_Default_RootConstants
		#define Sharpen_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) Sharpen_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct Sharpen_Default_RCSTRUCT
			enum {
				E_InvTexSizeX = 0,
				E_InvTexSizeY,
				E_Amount,
				Count
			};
		}
	}
#endif
}

namespace Pixelation {
#ifndef Pixelation_Default_RCSTRUCT
	#define Pixelation_Default_RCSTRUCT	{	\
		DirectX::XMFLOAT2 gTexSize;			\
		FLOAT			  gPixelSize;		\
	};
#endif

#ifdef HLSL
	#ifndef Pixelation_Default_RootConstants
		#define Pixelation_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) Pixelation_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct Pixelation_Default_RCSTRUCT
			enum {
				E_TexSize_X = 0,
				E_TexSize_Y,
				E_PixelSize,
				Count
			};
		}
	}
#endif
}

namespace IrradianceMap {
#ifndef IrradianceMap_ConvoluteSpecularIrradiance_RCSTRUCT
	#define IrradianceMap_ConvoluteSpecularIrradiance_RCSTRUCT {	\
		UINT	gMipLevel;											\
		FLOAT	gRoughness;											\
		FLOAT	gResolution;										\
	};
#endif

#ifdef HLSL
	typedef HDR_FORMAT	DiffuseIrradCubeMapFormat;
	typedef HDR_FORMAT	DiffuseIrradEquirectMapFormat;
	typedef HDR_FORMAT	EnvCubeMapFormat;
	typedef HDR_FORMAT	PrefilteredEnvCubeMapFormat;
	typedef HDR_FORMAT	PrefilteredEnvEquirectMapFormat;
	typedef HDR_FORMAT	EquirectMapFormat;
	typedef float2		IntegratedBrdfMapFormat;

	#ifndef IrradianceMap_ConvoluteSpecularIrradiance_RootConstants
		#define IrradianceMap_ConvoluteSpecularIrradiance_RootConstants(reg) cbuffer cbRootConstants : register(reg) IrradianceMap_ConvoluteSpecularIrradiance_RCSTRUCT
	#endif
#else
	static const DXGI_FORMAT DiffuseIrradCubeMapFormat			= HDR_FORMAT;
	static const DXGI_FORMAT DiffuseIrradEquirectMapFormat		= HDR_FORMAT;
	static const DXGI_FORMAT EnvCubeMapFormat					= HDR_FORMAT;
	static const DXGI_FORMAT PrefilteredEnvCubeMapFormat		= HDR_FORMAT;
	static const DXGI_FORMAT PrefilteredEnvEquirectMapFormat	= HDR_FORMAT;
	static const DXGI_FORMAT EquirectMapFormat					= HDR_FORMAT;
	static const DXGI_FORMAT IntegratedBrdfMapFormat			= DXGI_FORMAT_R16G16_FLOAT;

	namespace CubeMap {
		enum Type {
			E_Equirectangular = 0,
			E_EnvironmentCube,
			E_DiffuseIrradianceCube,
			E_PrefilteredIrradianceCube,
			Count
		};
	}

	namespace RootConstant {
		namespace ConvoluteSpecularIrradiance {
			struct Struct IrradianceMap_ConvoluteSpecularIrradiance_RCSTRUCT
			enum {
				E_MipLevel = 0,
				E_Roughness,
				E_Resolution,
				Count
			};
		}
	}
#endif
}

namespace EquirectangularConverter {
#ifndef EquirectangularConverter_ConvCubeToEquirect_RCSTRUCT
	#define EquirectangularConverter_ConvCubeToEquirect_RCSTRUCT {	\
		UINT gMipLevel;												\
	};
#endif
	
#ifdef HLSL
	#ifndef EquirectangularConverter_ConvCubeToEquirect_RootConstants
		#define EquirectangularConverter_ConvCubeToEquirect_RootConstants(reg) cbuffer cbRootConstants : register(reg) EquirectangularConverter_ConvCubeToEquirect_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace ConvCubeToEquirect {
			struct Struct EquirectangularConverter_ConvCubeToEquirect_RCSTRUCT
			enum {
				E_MipLevel = 0,
				Count
			};
		}
	}
#endif
}

namespace GammaCorrection {
#ifndef GammaCorrection_Default_RCSTRUCT
	#define GammaCorrection_Default_RCSTRUCT {	\
		FLOAT gGamma;							\
	};
#endif

#ifdef HLSL
	#ifndef GammaCorrection_Default_RootConstants
		#define GammaCorrection_Default_RootConstants(reg) cbuffer gRootConstants : register(reg) GammaCorrection_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct GammaCorrection_Default_RCSTRUCT
			enum {
				E_Gamma = 0,
				Count
			};
		}
	}
#endif
}

namespace BlurFilter {
	static const INT MaxBlurRadius = 17;

	enum FilterType {
		R8G8B8A8,
		R16,
		R16G16B16A16,
		R32G32B32A32,
		Count
	};

#ifndef BlurFilter_Default_RCSTRUCT
	#define BlurFilter_Default_RCSTRUCT {	\
		BOOL gHorizontal;					\
		BOOL gBilateral;					\
	};
#endif

#ifdef HLSL
	#ifndef BlurFilter_Default_RootConstants
		#define BlurFilter_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) BlurFilter_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct BlurFilter_Default_RCSTRUCT

			enum {
				E_Horizontal = 0,
				E_Bilateral,
				Count
			};
		}
	}
#endif

	namespace CS {
		static const INT MaxBlurRadius = 5;

		namespace ThreadGroup {
			namespace Default {
				enum {
					Size = 256
				};
			}
		}

	#ifndef BlurFilter_CS_Default_RCSTRUCT
		#define BlurFilter_CS_Default_RCSTRUCT {	\
			DirectX::XMFLOAT2 gDimension;			\
		};
	#endif

	#ifdef HLSL
		#ifndef BlurFilter_CS_Default_RootConstants
			#define BlurFilter_CS_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) BlurFilter_CS_Default_RCSTRUCT
		#endif
	#else
		namespace RootConstant {
			namespace Default {
				struct Struct BlurFilter_CS_Default_RCSTRUCT
					
				enum {
					E_DimensionX = 0,
					E_DimensionY,
					Count
				};
			}
		}
	#endif
	}
}

namespace Debug {
	static const INT MapSize = 5;

	namespace SampleMask {
		enum Type {
			RGB = 0,
			RG = 1 << 0,
			RRR = 1 << 1,
			GGG = 1 << 2,
			BBB = 1 << 3,
			AAA = 1 << 4,
			FLOAT = 1 << 5,
			UINT = 1 << 6
		};
	}

#ifndef Debug_DebugTexMap_RCSTRUCT
	#define Debug_DebugTexMap_RCSTRUCT {	\
		UINT gSampleMask0;					\
		UINT gSampleMask1;					\
		UINT gSampleMask2;					\
		UINT gSampleMask3;					\
		UINT gSampleMask4;					\
	};
#endif

#ifndef Debug_DebugCubeMap_RCSTRUCT
	#define Debug_DebugCubeMap_RCSTRUCT {	\
		FLOAT gMipLevel;					\
	};
#endif

#ifdef HLSL
	#ifndef Debug_DebugTexMap_RootConstants
		#define Debug_DebugTexMap_RootConstants(reg) cbuffer gRootConstants : register(reg) Debug_DebugTexMap_RCSTRUCT
	#endif

	#ifndef Debug_DebugCubeMap_RootConstants
		#define Debug_DebugCubeMap_RootConstants(reg) cbuffer cbRootConstants : register(reg) Debug_DebugCubeMap_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		struct Struct Debug_DebugTexMap_RCSTRUCT
		namespace DebugTexMap {
			enum {
				E_SampleMask0 = 0,
				E_SampleMask1,
				E_SampleMask2,
				E_SampleMask3,
				E_SampleMask4,
				Count
			};
		}

		namespace DebugCubeMap {
			struct Struct Debug_DebugCubeMap_RCSTRUCT
			enum {
				E_MipLevel = 0,
				Count
			};
		}
	}
#endif
	
}

namespace DXR_GeometryBuffer {
	static const UINT GeometryBufferCount = 32;
}

namespace GaussianFilter {
	namespace ThreadGroup {
		namespace Default {
			enum {
				Width	= 8,
				Height	= 8,
				Size	= Width * Height
			};
		}
	}

#ifndef GaussianFilter_Default_RCSTRUCT
	#define GaussianFilter_Default_RCSTRUCT {	\
		DirectX::XMUINT2  gTextureDim;			\
		DirectX::XMFLOAT2 gInvTextureDim;		\
	};
#endif

#ifdef HLSL
	#ifndef GaussianFilter_Default_RootConstants
		#define GaussianFilter_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) GaussianFilter_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct GaussianFilter_Default_RCSTRUCT

			enum {
				E_DimensionX = 0,
				E_DimensionY,
				E_InvDimensionX,
				E_InvDimensionY,
				Count
			};
		}
	}
#endif
}

namespace SVGF {
#ifdef HLSL
	typedef CONTRAST_VALUE_FORMAT	ValueMapFormat_Contrast;
	typedef HDR_COLOR_VALUE_FORMAT	ValueMapFormat_HDR;
	typedef CONTRAST_VALUE_FORMAT	ValueSquaredMeanMapFormat_Contrast;
	typedef HDR_COLOR_VALUE_FORMAT	ValueSquaredMeanMapFormat_HDR;

	typedef uint4					TsppSquaredMeanRayHitDistanceFormat;
	typedef float2					DepthPartialDerivativeMapFormat;
	typedef float2					LocalMeanVarianceMapFormat;
	typedef float					VarianceMapFormat;
	typedef float					RayHitDistanceFormat;
	typedef uint					TsppMapFormat;
	typedef float					DisocclusionBlurStrengthMapFormat;
#else
	const DXGI_FORMAT ValueMapFormat_Contrast				= CONTRAST_VALUE_FORMAT;
	const DXGI_FORMAT ValueMapFormat_HDR					= HDR_COLOR_VALUE_FORMAT;
	const DXGI_FORMAT ValueSquaredMeanMapFormat_Contrast	= CONTRAST_VALUE_FORMAT;
	const DXGI_FORMAT ValueSquaredMeanMapFormat_HDR			= HDR_COLOR_VALUE_FORMAT;

	const DXGI_FORMAT TsppSquaredMeanRayHitDistanceFormat	= DXGI_FORMAT_R16G16B16A16_UINT;
	const DXGI_FORMAT DepthPartialDerivativeMapFormat		= DXGI_FORMAT_R16G16_FLOAT;
	const DXGI_FORMAT LocalMeanVarianceMapFormat			= DXGI_FORMAT_R32G32_FLOAT;
	const DXGI_FORMAT VarianceMapFormat						= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT RayHitDistanceFormat					= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT TsppMapFormat							= DXGI_FORMAT_R8_UINT;
	const DXGI_FORMAT DisocclusionBlurStrengthMapFormat		= DXGI_FORMAT_R8_UNORM;
#endif
	namespace Default {
		namespace ThreadGroup {
			enum {
				Width	= 8,
				Height	= 8,
				Size	= Width * Height
			};
		}
	}

	namespace Atrous {
		namespace ThreadGroup {
			enum {
				Width	= 16,
				Height	= 16,
				Size	= Width * Height
		};
	}
}
}

namespace RTAO {
#ifdef HLSL
	typedef CONTRAST_VALUE_FORMAT				AOCoefficientMapFormat;
	typedef COMPACT_NORMAL_DEPTH_DXGI_FORMAT	NormalDepthMapFormat;
	typedef uint								TsppMapFormat;
	typedef float								CoefficientSquaredMeanMapFormat;
	typedef float								RayHitDistanceFormat;

	static const float RayHitDistanceOnMiss = 0.f;
	static const float InvalidAOCoefficientValue = -1.f;

	bool HasAORayHitAnyGeometry(float tHit) {
		return tHit != RayHitDistanceOnMiss;
	}
#else
	const DXGI_FORMAT AOCoefficientMapFormat			= CONTRAST_VALUE_FORMAT;
	const DXGI_FORMAT NormalDepthMapFormat				= COMPACT_NORMAL_DEPTH_DXGI_FORMAT;
	const DXGI_FORMAT TsppMapFormat						= DXGI_FORMAT_R8_UINT;
	const DXGI_FORMAT CoefficientSquaredMeanMapFormat	= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT RayHitDistanceFormat				= DXGI_FORMAT_R16_FLOAT;
#endif
}

namespace RaytracedReflection {
#ifdef HLSL
	typedef HDR_COLOR_VALUE_FORMAT	ReflectionMapFormat;
	typedef float					RayHitDistanceFormat;
	typedef uint					TsppMapFormat;
	typedef float					CoefficientSquaredMeanMapFormat;

	static const float InvalidReflectionAlphaValue = -1.f;
	static const float RayHitDistanceOnMiss = 0.f;

	bool HasReflectionRayHitAnyGeometry(float tHit) {
		return tHit != RayHitDistanceOnMiss;
	}
#else
	const DXGI_FORMAT ReflectionMapFormat				= HDR_COLOR_VALUE_FORMAT;
	const DXGI_FORMAT RayHitDistanceFormat				= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT TsppMapFormat						= DXGI_FORMAT_R8_UINT;
	const DXGI_FORMAT ReflectionSquaredMeanMapFormat	= DXGI_FORMAT_R8G8B8A8_UNORM;
#endif

	static const UINT InstanceMask = 0xFF;

	namespace Ray {
		enum Type {
			E_Radiance = 0,
			E_Shadow,
			Count
		};
	}

	namespace HitGroup {
		static const UINT Offset[Ray::Count] = {
			0, // Radiance ray
			1  // Shadow ray
		};
		static const UINT GeometryStride = Ray::Count;
	}

	namespace Miss {
		static const UINT Offset[Ray::Count] = {
			0, // Radiance ray
			1  // Shadow ray
		};
	}
}

namespace VolumetricLight {
	static const float InvalidFrustumVolumeAlphaValue = -1.f;

	namespace ThreadGroup {
		namespace CalcScatteringAndDensity {
			enum {
				Width = 8,
				Height = 8,
				Depth = 8,
				Size = Width * Height * Depth
			};
		}

		namespace AccumulateScattering {
			enum {
				Width = 8,
				Height = 8,
				Size = Width * Height
			};
		}
	}

#ifndef VolumetricLight_CalcScatteringAndDensity_RCSTRUCT
	#define VolumetricLight_CalcScatteringAndDensity_RCSTRUCT {	\
		FLOAT gNearZ;											\
		FLOAT gFarZ;											\
		FLOAT gDepthExponent;									\
		FLOAT gUniformDensity;									\
		FLOAT gAnisotropicCoefficient;							\
		UINT  gFrameCount;										\
	};
#endif
#ifndef VolumetricLight_AccumulateScattering_RCSTRUCT 
#define VolumetricLight_AccumulateScattering_RCSTRUCT {	\
		FLOAT gNearZ;									\
		FLOAT gFarZ;									\
		FLOAT gDepthExponent;							\
		FLOAT gDensityScale;							\
	};
#endif

#ifdef HLSL
	#ifndef VolumetricLight_CalcScatteringAndDensity_RootConstants
		#define VolumetricLight_CalcScatteringAndDensity_RootConstants(reg) cbuffer cbRootConstants : register (reg) VolumetricLight_CalcScatteringAndDensity_RCSTRUCT
	#endif
	#ifndef VolumetricLight_AccumulateScattering_RootConstants
		#define VolumetricLight_AccumulateScattering_RootConstants(reg) cbuffer cbRootConstants : register (reg) VolumetricLight_AccumulateScattering_RCSTRUCT
	#endif

	typedef float4 FrustumMapFormat;
	typedef float4 DebugMapFormat;

	bool IsValidFrustumVolume(float4 val) {
		return val.w == InvalidFrustumVolumeAlphaValue;
	}

	bool IsInvalidFrustumVolume(float4 val) {
		return val.w != InvalidFrustumVolumeAlphaValue;
	}
#else
	const DXGI_FORMAT FrustumMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const DXGI_FORMAT DebugMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	namespace RootConstant {
		namespace CalcScatteringAndDensity {
			struct Struct VolumetricLight_CalcScatteringAndDensity_RCSTRUCT

			enum {
				E_NearPlane = 0,
				E_FarPlane,
				E_DepthExponent,
				E_UniformDensity,
				E_AnisotropicCoefficient,
				E_FrameCount,
				Count
			};
		}

		namespace AccumulateScattering {
			struct Struct VolumetricLight_AccumulateScattering_RCSTRUCT

			enum {
				E_NearPlane = 0,
				E_FarPlane,
				E_DepthExponent,
				E_DensityScale,
				Count
			};
		}
	}
#endif
}

namespace MipmapGenerator {
#ifndef MipmapGenerator_Default_RCSTRUCT
	#define MipmapGenerator_Default_RCSTRUCT {	\
		DirectX::XMFLOAT2 gInvTexSize;			\
		DirectX::XMFLOAT2 gInvMipmapTexSize;	\
	};
#endif

#ifdef HLSL
	#ifndef MipmapGenerator_Default_RootConstants
		#define MipmapGenerator_Default_RootConstants(reg) cbuffer cbRootConstants : register(reg) MipmapGenerator_Default_RCSTRUCT
	#endif
#else
	namespace RootConstant {
		namespace Default {
			struct Struct MipmapGenerator_Default_RCSTRUCT
			enum {
				E_InvTexSizeW = 0,
				E_InvTexSizeH,
				E_InvMipmapTexSizeW,
				E_InvMipmapTexSizeH,
				Count
			};
		}
	}
#endif

	namespace ThreadGroup {
		enum {
			Width	= 8,
			Height	= 8,
			Size	= Width * Height
		};
	}
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

#include "ShadingConvention.inl"
