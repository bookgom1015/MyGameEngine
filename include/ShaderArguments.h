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
	static const FLOAT InvalidDepthValue = 1.0f;
	static const FLOAT InvalidNormDepthValue = 0.0f;

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
#else 
	static const DXGI_FORMAT AlbedoMapFormat			= DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT NormalMapFormat			= DXGI_FORMAT_R16G16B16A16_FLOAT;
	static const DXGI_FORMAT NormalDepthMapFormat		= COMPACT_NORMAL_DEPTH_DXGI_FORMAT;
	static const DXGI_FORMAT DepthMapFormat				= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	static const DXGI_FORMAT RMSMapFormat				= DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT VelocityMapFormat			= DXGI_FORMAT_R16G16_FLOAT;
	static const DXGI_FORMAT PositionMapFormat			= DXGI_FORMAT_R16G16B16A16_FLOAT;

	const FLOAT AlbedoMapClearValues[4]				= { 0.0f, 0.0f, 0.0f, 0.0f };
	const FLOAT NormalMapClearValues[4]				= { 0.0f, 0.0f, 0.0f, -1.0f };
	const FLOAT NormalDepthMapClearValues[4]		= { 0.0f, 0.0f, 0.0f, 0.0f };
	const FLOAT RMSMapClearValues[4]				= { 0.5f, 0.0f, 0.5f, 0.0f };
	const FLOAT VelocityMapClearValues[2]			= { 1000.0f, 1000.0f };
	const FLOAT ReprojNormalDepthMapClearValues[4]	= { 0.0f, 0.0f, 0.0f, 0.0f };
	const FLOAT PositionMapClearValues[4]			= { 0.0f, 0.0f, 0.0f, -1.0f };
#endif 
}

namespace ShadowMap {
#ifdef HLSL
	typedef float ShadowMapFormat;
#else 
	static const DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif 
}

namespace DxrShadowMap {
#ifdef HLSL
	typedef float ShadowMapFormat;
#else 
	static const DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_R16_UNORM;
#endif 
}

namespace DepthStencilBuffer {
#ifdef HLSL
	typedef float BufferFormat;
#else
	const DXGI_FORMAT BufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif

	static const FLOAT InvalidDepthValue = 1.0f;
}

namespace ToneMapping {
#ifdef HLSL
	typedef HDR_FORMAT IntermediateMapFormat;
#else
	const DXGI_FORMAT IntermediateMapFormat = HDR_FORMAT;
#endif
}

namespace Ssao {
#ifdef HLSL
	typedef float	AOCoefficientMapFormat;
	typedef float3	RandomVectorMapFormat;
#else
	static const DXGI_FORMAT AOCoefficientMapFormat	= DXGI_FORMAT_R16_UNORM;
	static const DXGI_FORMAT RandomVectorMapFormat	= DXGI_FORMAT_R8G8B8A8_SNORM;
#endif
}

namespace Ssr {
#ifdef HLSL
	typedef HDR_FORMAT SsrMapFormat;
#else
	static const DXGI_FORMAT SsrMapFormat = HDR_FORMAT;
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
	typedef HDR_FORMAT	DofMapFormat;
	typedef float		CocMapFormat;
#else
	static const DXGI_FORMAT DofMapFormat = HDR_FORMAT;
	static const DXGI_FORMAT CocMapFormat = DXGI_FORMAT_R16_FLOAT;
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

namespace DxrGeometryBuffer {
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
				Width = 8,
				Height = 8,
				Size = Width * Height
			};
		}
	}

	namespace Atrous {
		namespace ThreadGroup {
			enum {
				Width = 16,
				Height = 16,
				Size = Width * Height
		};
	}
}
}

namespace Rtao {
#ifdef HLSL
	typedef CONTRAST_VALUE_FORMAT				AOCoefficientMapFormat;
	typedef COMPACT_NORMAL_DEPTH_DXGI_FORMAT	NormalDepthMapFormat;
	typedef uint								TsppMapFormat;
	typedef float								CoefficientSquaredMeanMapFormat;
	typedef float								RayHitDistanceFormat;
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
#else
	const DXGI_FORMAT ReflectionMapFormat				= HDR_COLOR_VALUE_FORMAT;
	const DXGI_FORMAT RayHitDistanceFormat				= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT TsppMapFormat						= DXGI_FORMAT_R8_UINT;
	const DXGI_FORMAT ReflectionSquaredMeanMapFormat	= DXGI_FORMAT_R8G8B8A8_UNORM;
#endif

	namespace Ray {
		enum Type {
			E_Radiance = 0,
			E_Shadow,
			Count
		};
	}

	static const UINT InstanceMask = 0xFF;

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

namespace MipmapGenerator {
	namespace ThreadGroup {
		enum {
			Width	= 8,
			Height	= 8,
			Size	= Width * Height
		};
	}
}
