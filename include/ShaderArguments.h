#pragma once

namespace GBuffer {
#ifdef HLSL
	typedef float4	AlbedoMapFormat;
	typedef float4	NormalMapFormat;
	typedef uint	NormalDepthMapFormat;
	typedef float	DepthMapFormat;
	typedef float4	RMSMapFormat;
	typedef float2	VelocityMapFormat;
	typedef uint	ReprojNormalDepthMapFormat;
#else 
	static const DXGI_FORMAT AlbedoMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
	static const DXGI_FORMAT NormalDepthMapFormat = DXGI_FORMAT_R32_UINT;
	static const DXGI_FORMAT DepthMapFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	static const DXGI_FORMAT RMSMapFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const DXGI_FORMAT VelocityMapFormat = DXGI_FORMAT_R16G16_FLOAT;
	static const DXGI_FORMAT ReprojNormalDepthMapFormat = DXGI_FORMAT_R32_UINT;
#endif 
}

namespace ShadowMap {
#ifdef HLSL
	typedef float ShadowMapFormat;
#else 
	static const DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif 
}

namespace Ssao {
#ifdef HLSL
	typedef float AOCoefficientMapFormat;
	typedef float3 RandomVectorMapFormat;
#else
	static const DXGI_FORMAT AOCoefficientMapFormat = DXGI_FORMAT_R16_UNORM;
	static const DXGI_FORMAT RandomVectorMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
#endif
}

namespace DebugMap {
	static const int MapSize = 5;

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
				Width = 8,
				Height = 8,
				Size = Width * Height
			};
		}
	}
}

namespace Rtao {
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

namespace MipmapGenerator {
	namespace ThreadGroup {
		enum {
			Width = 8,
			Height = 8,
			Size = Width * Height
		};
	}
}