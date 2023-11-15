#pragma once

#ifdef HLSL
	#ifndef HDR_FORMAT
	#define HDR_FORMAT float4
	#endif
	
	#ifndef SDR_FORMAT
	#define SDR_FORMAT float4
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
#endif

namespace GBuffer {
	static const float RayHitDistanceOnMiss = 1.0f;

#ifdef HLSL
	typedef float4	AlbedoMapFormat;
	typedef float4	NormalMapFormat;
	typedef uint	NormalDepthMapFormat;
	typedef float	DepthMapFormat;
	typedef float4	RMSMapFormat;
	typedef float2	VelocityMapFormat;
	typedef uint	ReprojNormalDepthMapFormat;
#else 
	static const DXGI_FORMAT AlbedoMapFormat			= DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT NormalMapFormat			= DXGI_FORMAT_R8G8B8A8_SNORM;
	static const DXGI_FORMAT NormalDepthMapFormat		= COMPACT_NORMAL_DEPTH_DXGI_FORMAT;
	static const DXGI_FORMAT DepthMapFormat				= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	static const DXGI_FORMAT RMSMapFormat				= DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT VelocityMapFormat			= DXGI_FORMAT_R16G16_FLOAT;
	static const DXGI_FORMAT ReprojNormalDepthMapFormat	= COMPACT_NORMAL_DEPTH_DXGI_FORMAT;
#endif 
}

namespace ShadowMap {
#ifdef HLSL
	typedef float ShadowMapFormat;
#else 
	static const DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif 
}

namespace DepthStencilBuffer {
#ifdef HLSL
	typedef float BufferFormat;
#else
	const DXGI_FORMAT BufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif

	static const float InvalidDepthValue = 1.0f;
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
	typedef float4 SsrMapFormat;
#else
	static const DXGI_FORMAT SsrMapFormat = HDR_FORMAT;
#endif
}

namespace IrradianceMap {
#ifdef HLSL
	typedef float4 DiffuseIrradCubeMapFormat;
	typedef float4 DiffuseIrradEquirectMapFormat;
	typedef float4 EnvCubeMapFormat;
	typedef float4 PrefilteredEnvCubeMapFormat;
	typedef float4 PrefilteredEnvEquirectMapFormat;
	typedef float4 EquirectMapFormat;
	typedef float2 IntegratedBrdfMapFormat;
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

namespace RaytracedReflection{
#ifdef HLSL
	typedef float4 ReflectionMapFormat;
#else
	static const DXGI_FORMAT ReflectionMapFormat = HDR_FORMAT;
#endif
}

namespace DebugMap {
	static const int MapSize = 5;

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
	static const int MaxBlurRadius = 5;

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

namespace Rtao {
#ifdef HLSL
	typedef float	AOCoefficientMapFormat;
	typedef uint	NormalDepthMapFormat;
	typedef float2	DepthPartialDerivativeMapFormat;
	typedef uint4	TsppCoefficientSquaredMeanRayHitDistanceFormat;
	typedef float	DisocclusionBlurStrengthMapFormat;
	typedef uint	TsppMapFormat;
	typedef float	CoefficientSquaredMeanMapFormat;
	typedef float	RayHitDistanceFormat;
	typedef float2	LocalMeanVarianceMapFormat;
	typedef float	VarianceMapFormat;
#else
	const DXGI_FORMAT AOCoefficientMapFormat							= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT NormalDepthMapFormat								= COMPACT_NORMAL_DEPTH_DXGI_FORMAT;
	const DXGI_FORMAT DepthPartialDerivativeMapFormat					= DXGI_FORMAT_R16G16_FLOAT;
	const DXGI_FORMAT TsppCoefficientSquaredMeanRayHitDistanceFormat	= DXGI_FORMAT_R16G16B16A16_UINT;
	const DXGI_FORMAT DisocclusionBlurStrengthMapFormat					= DXGI_FORMAT_R8_UNORM;
	const DXGI_FORMAT TsppMapFormat										= DXGI_FORMAT_R8_UINT;
	const DXGI_FORMAT CoefficientSquaredMeanMapFormat					= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT RayHitDistanceFormat								= DXGI_FORMAT_R16_FLOAT;
	const DXGI_FORMAT LocalMeanVarianceMapFormat						= DXGI_FORMAT_R16G16_FLOAT;
	const DXGI_FORMAT VarianceMapFormat									= DXGI_FORMAT_R16_FLOAT;
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

namespace MipmapGenerator {
	namespace ThreadGroup {
		enum {
			Width	= 8,
			Height	= 8,
			Size	= Width * Height
		};
	}
}