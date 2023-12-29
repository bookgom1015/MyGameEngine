#ifndef __TEMPORALSUPERSAMPLINGBLENDWIDTHCURRENTFRAMECS_HLSL__
#define __TEMPORALSUPERSAMPLINGBLENDWIDTHCURRENTFRAMECS_HLSL__

// 2nd stage of temporal supersampling.
// Blends current frame values with values reprojected from previous frame in stage 1.

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"
#include "Rtao.hlsli"

ConstantBuffer<TemporalSupersamplingBlendWithCurrentFrameConstants> cbBlend : register(b0);

Texture2D<SVGF::F1ValueMapFormat>							gi_CurrentFrameValue						: register(t0);
Texture2D<SVGF::LocalMeanVarianceMapFormat>					gi_CurrentFrameLocalMeanVariance			: register(t1);
Texture2D<SVGF::RayHitDistanceFormat>						gi_CurrentFrameRayHitDistance				: register(t2);
Texture2D<SVGF::TsppF1ValueSquaredMeanRayHitDistanceFormat>	gi_ReprojTsppValueSquaredMeanRayHitDist		: register(t3);

RWTexture2D<SVGF::F1ValueMapFormat>						gio_Value				: register(u0);
RWTexture2D<SVGF::TsppMapFormat>						gio_Tspp				: register(u1);
RWTexture2D<SVGF::F1ValueSquaredMeanMapFormat>			gio_ValueSquaredMean	: register(u2);
RWTexture2D<SVGF::RayHitDistanceFormat>					gio_RayHitDistance		: register(u3);
RWTexture2D<SVGF::VarianceMapFormat>					go_Variance				: register(u4);
RWTexture2D<SVGF::DisocclusionBlurStrengthMapFormat>	go_BlurStrength			: register(u5);

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	const uint4 EncodedCachedValues = gi_ReprojTsppValueSquaredMeanRayHitDist[DTid];
	uint tspp = EncodedCachedValues.x;
	const float4 CachedValues = float4(tspp, f16tof32(EncodedCachedValues.yzw));

	bool isCurrentFrameValueActive = true;
	if (cbBlend.CheckerboardEnabled) {
		bool isEvenPixel = ((DTid.x + DTid.y) & 1) == 0;
		isCurrentFrameValueActive = cbBlend.CheckerboardEvenPixelActivated == isEvenPixel;
	}

	float value = isCurrentFrameValueActive ? gi_CurrentFrameValue[DTid] : Rtao::InvalidAOCoefficientValue;
	const bool IsValidValue = value != Rtao::InvalidAOCoefficientValue;
	float valueSquaredMean = IsValidValue ? value * value : Rtao::InvalidAOCoefficientValue;
	float rayHitDistance = Rtao::InvalidAOCoefficientValue;
	float variance = Rtao::InvalidAOCoefficientValue;

	if (tspp > 0) {
		const uint MaxTspp = 1 / cbBlend.MinSmoothingFactor;
		tspp = IsValidValue ? min(tspp + 1, MaxTspp) : tspp;

		float cachedValue = CachedValues.y;

		const float2 LocalMeanVariance = gi_CurrentFrameLocalMeanVariance[DTid];
		const float LocalMean = LocalMeanVariance.x;
		const float LocalVariance = LocalMeanVariance.y;
		if (cbBlend.ClampCachedValues) {
			const float LocalStdDev = max(cbBlend.StdDevGamma * sqrt(LocalVariance), cbBlend.ClampingMinStdDevTolerance);
			const float NonClampedCachedValue = cachedValue;
		
			// Clamp value to mean +/- std.dev of local neighborhood to supress ghosting on value changing due to other occluder movements.
			// Ref: Salvi2016, Temporal-Super-Sampling
			cachedValue = clamp(cachedValue, LocalMean - LocalStdDev, LocalMean + LocalStdDev);
		
			// Scale down the tspp based on how strongly the cached value got clamped to give more weight to new smaples.
			float tsppScale = saturate(cbBlend.ClampDifferenceToTsppScale * abs(cachedValue - NonClampedCachedValue));
			tspp = lerp(tspp, 0, tsppScale);
		}
		const float InvTspp = 1.0 / tspp;
		float a = cbBlend.ForceUseMinSmoothingFactor ? cbBlend.MinSmoothingFactor : max(InvTspp, cbBlend.MinSmoothingFactor);
		const float MaxSmoothingFactor = 1;
		a = min(a, MaxSmoothingFactor);

		// TODO: use average weighting instead of exponential for the first few samples
		//  to even out the weights for the noisy start instead of giving first samples mush more weight than the rest.
		// Ref: Koskela2019, Blockwise Multi-Order Feature Regression for Real-Time Path-Tracing Reconstruction

		// Value.
		value = IsValidValue ? lerp(cachedValue, value, a) : cachedValue;

		// Value Squared Mean.
		float cachedSquaredMeanValue = CachedValues.z;
		valueSquaredMean = IsValidValue ? lerp(cachedSquaredMeanValue, valueSquaredMean, a) : cachedSquaredMeanValue;

		// Variance.
		float temporalVariance = valueSquaredMean - value * value;
		temporalVariance = max(0, temporalVariance); // Ensure variance doesn't go negative due to imprecision.
		variance = tspp >= cbBlend.MinTsppToUseTemporalVariance ? temporalVariance : LocalVariance;
		variance = max(0.1, variance);

		// RayHitDistance.
		rayHitDistance = IsValidValue ? gi_CurrentFrameRayHitDistance[DTid] : 0;
		float cachedRayHitDistance = CachedValues.w;
		rayHitDistance = IsValidValue ? lerp(cachedRayHitDistance, rayHitDistance, a) : cachedRayHitDistance;
	}
	else if (IsValidValue) {
		tspp = 1;
		value = value;

		rayHitDistance = gi_CurrentFrameRayHitDistance[DTid];
		variance = gi_CurrentFrameLocalMeanVariance[DTid].y;
		valueSquaredMean = valueSquaredMean;
	}

	const float TsppRatio = min(tspp, cbBlend.BlurStrengthMaxTspp) / float(cbBlend.BlurStrengthMaxTspp);
	const float BlurStrength = pow(1 - TsppRatio, cbBlend.BlurDecayStrength);

	gio_Tspp[DTid] = tspp;
	gio_Value[DTid] = value;
	gio_ValueSquaredMean[DTid] = valueSquaredMean;
	gio_RayHitDistance[DTid] = rayHitDistance;
	go_Variance[DTid] = variance;
	go_BlurStrength[DTid] = BlurStrength;
}

#endif // __TEMPORALSUPERSAMPLINGBLENDWIDTHCURRENTFRAMECS_HLSL__