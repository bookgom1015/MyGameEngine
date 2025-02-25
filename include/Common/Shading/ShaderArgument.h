#pragma once

#include <Windows.h>

namespace ShaderArgument {
	namespace GBuffer {
		namespace Dither {
			static FLOAT MaxDistance = 0.5f;
			static FLOAT MinDistance = 0.1f;
		}
	}

	namespace Bloom {
		static FLOAT HighlightThreshold = 0.99f;
		static INT BlurCount = 3;
	}

	namespace DepthOfField {
		static FLOAT FocusRange = 8.f;
		static FLOAT FocusingSpeed = 8.f;
		static FLOAT BokehRadius = 2.f;
		static FLOAT CoCMaxDevTolerance = 0.8f;
		static FLOAT HighlightPower = 4.f;
		static INT   SampleCount = 4;
		static UINT  BlurCount = 1;
	}

	namespace SSR {
		static FLOAT MaxDistance = 32.f;
		static INT BlurCount = 3;

		namespace View {
			static FLOAT RayLength = 0.5f;
			static FLOAT NoiseIntensity = 0.01f;
			static INT StepCount = 16;
			static INT BackStepCount = 8;
			static FLOAT DepthThreshold = 1.f;
		}

		namespace Screen {
			static FLOAT Resolution = 0.25f;
			static FLOAT Thickness = 0.5f;
		}
	}

	namespace MotionBlur {
		static FLOAT Intensity = 0.01f;
		static FLOAT Limit = 0.005f;
		static FLOAT DepthBias = 0.05f;
		static INT SampleCount = 10;
	}

	namespace TemporalAA {
		static FLOAT ModulationFactor = 0.8f;
	}

	namespace SSAO {
		static INT SampleCount = 14;
		static INT BlurCount = 3;
	}

	namespace ToneMapping {
		static FLOAT Exposure = 1.4f;
	}

	namespace GammaCorrection {
		static FLOAT Gamma = 2.2f;
	}

	namespace DxrShadowMap {
		static INT BlurCount = 3;
	}

	namespace Debug {
		static BOOL ShowCollisionBox = FALSE;
	}

	namespace IrradianceMap {
		static BOOL ShowIrradianceCubeMap = FALSE;
		static FLOAT MipLevel = 0.f;
	}

	namespace Pixelization {
		static FLOAT PixelSize = 5.f;
	}

	namespace Sharpen {
		static FLOAT Amount = 0.8f;
	}

	namespace SVGF {
		static BOOL QuarterResolutionAO = FALSE;

		static BOOL CheckerboardGenerateRaysForEvenPixels = FALSE;
		static BOOL CheckerboardSamplingEnabled = FALSE;

		namespace TemporalSupersampling {
			static UINT MaxTspp = 33;

			namespace ClampCachedValues {
				static BOOL UseClamping = TRUE;
				static FLOAT StdDevGamma = 0.6f;
				static FLOAT MinStdDevTolerance = 0.05f;
				static FLOAT DepthSigma = 1.f;
			}

			static FLOAT ClampDifferenceToTsppScale = 4.f;
			static UINT MinTsppToUseTemporalVariance = 4;
			static UINT LowTsppMaxTspp = 12;
			static FLOAT LowTsppDecayConstant = 1.f;
		}

		namespace AtrousWaveletTransformFilter {
			static FLOAT ValueSigma = 1.f;
			static FLOAT DepthSigma = 1.f;
			static FLOAT DepthWeightCutoff = 0.2f;
			static FLOAT NormalSigma = 64.f;
			static FLOAT MinVarianceToDenoise = 0.f;
			static BOOL UseSmoothedVariance = FALSE;
			static BOOL PerspectiveCorrectDepthInterpolation = TRUE;
			static BOOL UseAdaptiveKernelSize = TRUE;
			static BOOL KernelRadiusRotateKernelEnabled = TRUE;
			static INT KernelRadiusRotateKernelNumCycles = 3;
			static INT FilterMinKernelWidth = 3;
			static FLOAT FilterMaxKernelWidthPercentage = 1.5f;
			static FLOAT AdaptiveKernelSizeRayHitDistanceScaleFactor = 0.02f;
			static FLOAT AdaptiveKernelSizeRayHitDistanceScaleExponent = 2.f;
		}
	}

	namespace RTAO {
		static FLOAT OcclusionRadius = 22.f;
		static FLOAT OcclusionFadeStart = 1.f;
		static FLOAT OcclusionFadeEnd = 22.f;
		static FLOAT OcclusionEpsilon = 0.05f;
		static UINT SampleCount = 2;

		namespace Denoiser {
			static BOOL UseSmoothingVariance = TRUE;
			static BOOL FullscreenBlur = TRUE;
			static BOOL DisocclusionBlur = TRUE;
			static UINT LowTsppBlurPasses = 3;
		}
	}

	namespace RaytracedReflection {
		static BOOL CheckerboardSamplingEnabled = FALSE;

		static FLOAT ReflectionRadius = 22.f;

		namespace Denoiser {
			static BOOL UseSmoothingVariance = TRUE;
			static BOOL FullscreenBlur = TRUE;
			static BOOL DisocclusionBlur = TRUE;
			static UINT LowTsppBlurPasses = 4;
		}
	}

	namespace VolumetricLight {
		static FLOAT DepthExponent = 4.f;
		static FLOAT UniformDensity = 0.1f;
		static FLOAT AnisotropicCoefficient = 0.f;
		static FLOAT DensityScale = 0.01f;
	}
}