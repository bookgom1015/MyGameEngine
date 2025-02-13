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
#endif 
}

namespace Shadow {
#ifdef HLSL
	typedef float	ZDepthMapFormat;
	typedef uint	ShadowMapFormat;
	typedef float4	DebugMapFormat;
	typedef float	VSDepthCubeMapFormat;
	typedef float	FaceIDCubeMapFormat;

	static const float	InvalidVSDepth	= -1.f;
	static const uint	InvalidFaceID	= 255;
#else 
	static const DXGI_FORMAT ZDepthMapFormat		= DXGI_FORMAT_D32_FLOAT;
	static const DXGI_FORMAT ShadowMapFormat		= DXGI_FORMAT_R16_UINT;
	static const DXGI_FORMAT DebugMapFormat			= DXGI_FORMAT_R8G8B8A8_SNORM;
	static const DXGI_FORMAT VSDepthCubeMapFormat	= DXGI_FORMAT_R16_FLOAT;
	static const DXGI_FORMAT FaceIDCubeMapFormat	= DXGI_FORMAT_R16_FLOAT;

	const FLOAT VSDepthCubeMapFormatClearValues[4]	= { -1.f, 0.f, 0.f, 0.f };
	const FLOAT FaceIDCubeMapClearValues[4]			= { 255.f, 0.f, 0.f, 0.f };
#endif 
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

namespace ToneMapping {
#ifdef HLSL
	typedef HDR_FORMAT IntermediateMapFormat;
#else
	const DXGI_FORMAT IntermediateMapFormat = HDR_FORMAT;
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
#endif
}

namespace Bloom {
#ifdef HLSL
	typedef HDR_FORMAT BloomMapFormat;
#else
	static const DXGI_FORMAT BloomMapFormat = HDR_FORMAT;
#endif
}

namespace DepthOfField {
#ifdef HLSL
	typedef float	CoCMapFormat;
	typedef float	FocalDistanceBufferFormat;
#else
	static const DXGI_FORMAT CoCMapFormat = DXGI_FORMAT_R16_FLOAT;
#endif
}

namespace IrradianceMap {
#ifdef HLSL
	typedef HDR_FORMAT	DiffuseIrradCubeMapFormat;
	typedef HDR_FORMAT	DiffuseIrradEquirectMapFormat;
	typedef HDR_FORMAT	EnvCubeMapFormat;
	typedef HDR_FORMAT	PrefilteredEnvCubeMapFormat;
	typedef HDR_FORMAT	PrefilteredEnvEquirectMapFormat;
	typedef HDR_FORMAT	EquirectMapFormat;
	typedef float2		IntegratedBrdfMapFormat;
#else
	static const DXGI_FORMAT DiffuseIrradCubeMapFormat			= HDR_FORMAT;
	static const DXGI_FORMAT DiffuseIrradEquirectMapFormat		= HDR_FORMAT;
	static const DXGI_FORMAT EnvCubeMapFormat					= HDR_FORMAT;
	static const DXGI_FORMAT PrefilteredEnvCubeMapFormat		= HDR_FORMAT;
	static const DXGI_FORMAT PrefilteredEnvEquirectMapFormat	= HDR_FORMAT;
	static const DXGI_FORMAT EquirectMapFormat					= HDR_FORMAT;
	static const DXGI_FORMAT IntegratedBrdfMapFormat			= DXGI_FORMAT_R16G16_FLOAT;
#endif
}

namespace BlurFilter {
	enum FilterType {
		R8G8B8A8,
		R16,
		R16G16B16A16,
		R32G32B32A32,
		Count
	};
}

namespace DebugMap {
	static const INT MapSize = 5;

	namespace SampleMask {
		enum Type {
			RGB		= 0,
			RG		= 1 << 0,
			RRR		= 1 << 1,
			GGG		= 1 << 2,
			BBB		= 1 << 3,
			AAA		= 1 << 4,
			FLOAT	= 1 << 5,
			UINT	= 1 << 6
		};
	}
}

namespace DXR_GeometryBuffer {
	static const UINT GeometryBufferCount = 32;
}

namespace BlurFilterCS {
	static const INT MaxBlurRadius = 5;

	namespace ThreadGroup {
		enum {
			Size = 256
		};
	}
}

namespace GaussianFilter {
	namespace Default {
		namespace ThreadGroup {
			enum {
				Width	= 8,
				Height	= 8,
				Size	= Width * Height
			};
		}
	}
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
#ifdef HLSL
	typedef float4 FrustumMapFormat;
	typedef float4 DebugMapFormat;
#else
	const DXGI_FORMAT FrustumMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	const DXGI_FORMAT DebugMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
#endif
	namespace CalcScatteringAndDensity {
		namespace ThreadGroup {
			enum {
				Width	= 8,
				Height	= 8,
				Depth	= 8,
				Size	= Width * Height * Depth
			};
		}
	}

	namespace AccumulateScattering {
		namespace ThreadGroup {
			enum {
				Width	= 8,
				Height	= 8,
				Size	= Width * Height
			};
		}
	}
}

namespace MipmapGenerator {
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