#ifndef __ATROUSWAVELETTRANFORMFILTERCS_HLSLI__
#define __ATROUSWAVELETTRANFORMFILTERCS_HLSLI__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Kernels.hlsli"
#include "Rtao.hlsli"

ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer> cbAtrous : register(b0);

Texture2D<SVGF::F1ValueMapFormat>					gi_Value					: register(t0);
Texture2D<GBuffer::NormalDepthMapFormat>			gi_NormalDepth				: register(t1);
Texture2D<SVGF::VarianceMapFormat>					gi_Variance					: register(t2);
Texture2D<SVGF::RayHitDistanceFormat>				gi_HitDistance				: register(t3);
Texture2D<SVGF::DepthPartialDerivativeMapFormat>	gi_DepthPartialDerivative	: register(t4);
Texture2D<SVGF::TsppMapFormat>						gi_Tspp						: register(t5);

RWTexture2D<SVGF::F1ValueMapFormat>			go_FilteredValue			: register(u0);

float DepthThreshold(float depth, float2 ddxy, float2 pixelOffset) {
	float depthThreshold;
	if (cbAtrous.PerspectiveCorrectDepthInterpolation) {
		float2 newDdxy = RemapDdxy(depth, ddxy, pixelOffset);
		depthThreshold = dot(1, abs(newDdxy));
	}
	else {
		depthThreshold = dot(1, abs(pixelOffset * ddxy));
	}
	return depthThreshold;
}

void AddFilterContribution(
		inout float weightedValueSum,
		inout float weightSum,
		float value,
		float stdDeviation,
		float depth,
		float3 normal,
		float2 ddxy,
		uint row,
		uint col,
		uint2 kernelStep,
		uint2 DTid) {
	const float ValueSigma = cbAtrous.ValueSigma;
	const float NormalSigma = cbAtrous.NormalSigma;
	const float DepthSigma = cbAtrous.DepthSigma;

	int2 pixelOffset;
	float kernelWidth;
	float varianceScale = 1;

	pixelOffset = int2(row - FilterKernel::Radius, col - FilterKernel::Radius) * kernelStep;
	int2 id = int2(DTid) + pixelOffset;

	if (!IsWithinBounds(id, cbAtrous.TextureDim)) return;

	float iDepth;
	float3 iNormal;
	DecodeNormalDepth(gi_NormalDepth[id], iNormal, iDepth);

	float iValue = gi_Value[id];

	bool isValidValue = iValue != Rtao::InvalidAOCoefficientValue;
	if (!isValidValue || iDepth == GBuffer::InvalidNormDepthValue) return;

	// Calculate a weight for the neighbor's contribution.
	float w;
	{
		// Value based weight.
		// Lower value tolerance for the neighbors further apart. Prevents overbluring shapr value transition.
		const float ErrorOffset = 0.005;
		float valueSigmaDistCoef = 1.0 / length(pixelOffset);
		float e_x = -abs(value - iValue) / (valueSigmaDistCoef * ValueSigma * stdDeviation + ErrorOffset);
		float w_x = exp(e_x);

		// Normal based weight.
		float w_n = pow(max(0, dot(normal, iNormal)), NormalSigma);

		// Depth based weight.
		float w_d;
		{
			float2 pixelOffsetForDepth = pixelOffset;

			// Account for sample offset in bilateral downsampled partial depth derivative buffer.
			if (cbAtrous.UsingBilateralDownsamplingBuffers) {
				float2 offsetSign = sign(pixelOffset);
				pixelOffsetForDepth = pixelOffset + offsetSign * float2(0.5, 0.5);
			}

			float depthFloatPrecision = FloatPrecision(max(depth, iDepth), cbAtrous.DepthNumMantissaBits);
			float depthThreshold = DepthThreshold(depth, ddxy, pixelOffsetForDepth);
			float depthTolerance = DepthSigma * depthThreshold + depthFloatPrecision;
			float delta = abs(depth - iDepth);
			// Avoid distinguishing initial values up to the float precision. Gets rid of banding due to low depth precision format.
			delta = max(0, delta - depthFloatPrecision);
			w_d = exp(-delta / depthTolerance);

			// Scale down contributions for samples beyond tolerance, but completely disable contribution for samples too far away.
			w_d *= w_d >= cbAtrous.DepthWeightCutoff;
		}

		// Filter kernel weight.
		float w_h = FilterKernel::Kernel[row][col];

		// Final weight.
		w = w_h * w_n * w_x * w_d;
	}

	weightedValueSum += w * iValue;
	weightSum += w;
}

[numthreads(SVGF::Atrous::ThreadGroup::Width, SVGF::Atrous::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	if (!IsWithinBounds(DTid, cbAtrous.TextureDim)) return;

	// Initialize values to the current pixel / center filter kernel value.
	float value = gi_Value[DTid];

	float3 normal;
	float depth;
	DecodeNormalDepth(gi_NormalDepth[DTid], normal, depth);

	bool isValidValue = value != Rtao::InvalidAOCoefficientValue;
	float filteredValue = value;
	float variance = gi_Variance[DTid];

	if (depth != GBuffer::InvalidNormDepthValue) {
		float2 ddxy = gi_DepthPartialDerivative[DTid];
		float weightSum = 0;
		float weightedValueSum = 0;
		float stdDeviation = 1;

		if (isValidValue) {
			float w = FilterKernel::Kernel[FilterKernel::Radius][FilterKernel::Radius];
			weightSum = w;
			weightedValueSum = weightSum * value;
			stdDeviation = sqrt(variance);
		}

		// Adaptive kernel size
		// Scale the kernel span based on AO ray hit distance.
		// This helps filter out lower frequency noise, a.k.a biling artifacts.
		// Ref: [RTGCH19]
		uint2 kernelStep = 0;
		if (cbAtrous.UseAdaptiveKernelSize && isValidValue) {
			float avgRayHitDistance = gi_HitDistance[DTid];

			float perPixelViewAngle = cbAtrous.FovY / cbAtrous.TextureDim.y;
			float tan_a = tan(perPixelViewAngle);
			float2 projectedSurfaceDim = ApproximateProjectedSurfaceDimensionsPerPixel(depth, ddxy, tan_a);

			// Calculate a kernel width as a ratio of hitDistance / projected surface dim per pixel.
			// Apply a non-linear factor based on relative rayHitDistance.
			// This is because average ray hit distance grows large fast if the closeby occluders cover only part if the hemisphere.
			// Having a smaller kernel for such cases helps preserve occlusion detail.
			float t = min(avgRayHitDistance / 22.0, 1); // 22 was seleted emprically.
			float k = cbAtrous.RayHitDistanceToKernelWidthScale * pow(t, cbAtrous.RayHitDistanceToKernelSizeScaleExponent);
			kernelStep = max(1, round(k * avgRayHitDistance / projectedSurfaceDim));

			uint2 targetKernelStep = clamp(kernelStep, (cbAtrous.MinKernelWidth - 1) / 2, (cbAtrous.MaxKernelWidth - 1) / 2);

			// TODO: additional options to explore
			// - non-uniform X, Y kernel radius cause visible streaking. Use same ratio across both X, Y? That may overblur one dimension at sharp angles.
			// - use larger kernel on lower tspp.
			// - use varying number of cycles for better spatial coverage over time, depending on the target kernel step. More cycles on larget kernels.
			uint2 adjustedKernelStep = lerp(1, targetKernelStep, cbAtrous.KernelRadiusLerfCoef);
			kernelStep = adjustedKernelStep;
		}

		if (variance >= cbAtrous.MinVarianceToDenoise) {
			// Add contributions from the neighborhood.
			[unroll]
			for (UINT r = 0; r < FilterKernel::Width; ++r) {
				[unroll]
				for (UINT c = 0; c < FilterKernel::Width; ++c) {
					if (r != FilterKernel::Radius || c != FilterKernel::Radius) {
						AddFilterContribution(
							weightedValueSum,
							weightSum,
							value,
							stdDeviation,
							depth,
							normal,
							ddxy,
							r, c,
							kernelStep,
							DTid
						);
					}
				}
			}
		}

		float smallValue = 0.000001;
		if (weightSum > smallValue) filteredValue = weightedValueSum / weightSum;
		else filteredValue = Rtao::InvalidAOCoefficientValue;
	}

	go_FilteredValue[DTid] = filteredValue;
}

#endif // __ATROUSWAVELETTRANFORMFILTERCS_HLSLI__