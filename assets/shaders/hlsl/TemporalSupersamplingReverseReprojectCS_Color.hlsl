// [ Descriptions ]
//  Stage 1 of Temporal-Supersampling.
//  Samples temporal cache via vectors/reserve reprojection.
//  If no valid values have been reterived from the cache, the tspp is set to 0.

#ifndef __TEMPORALSUPERSAMPLINGREVERSEREPROJECTCS_HLSL__
#define __TEMPORALSUPERSAMPLINGREVERSEREPROJECTCS_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

#include "ValuePackaging.hlsli"
#include "CrossBilateralWeights.hlsli"

ConstantBuffer<ConstantBuffer_CrossBilateralFilter> cb_Reproject : register (b0);

cbuffer cbRootConstants : register (b1) {
	uint2	gTextureDim;
	float2	gInvTextureDim;
};

Texture2D<GBuffer::NormalDepthMapFormat>				gi_CurrentFrameNormalDepth	: register(t0);
Texture2D<SVGF::DepthPartialDerivativeMapFormat>		gi_DepthPartialDerivative	: register(t1);
Texture2D<GBuffer::NormalDepthMapFormat>				gi_ReprojectedNormalDepth	: register(t2);
Texture2D<GBuffer::NormalDepthMapFormat>				gi_CachedNormalDepth		: register(t3);
Texture2D<GBuffer::VelocityMapFormat>					gi_Velocity					: register(t4);
Texture2D<SVGF::ValueMapFormat_HDR>						gi_CachedValue				: register(t5);
Texture2D<SVGF::ValueSquaredMeanMapFormat_HDR>			gi_CachedValueSquaredMean	: register(t7);
Texture2D<SVGF::TsppMapFormat>							gi_CachedTspp				: register(t6);
Texture2D<SVGF::RayHitDistanceFormat>					gi_CachedRayHitDistance		: register(t8);

RWTexture2D<SVGF::TsppMapFormat>						go_CachedTspp				: register(u0);
RWTexture2D<SVGF::ValueMapFormat_HDR>					go_CachedValue				: register(u1);
RWTexture2D<SVGF::ValueSquaredMeanMapFormat_HDR>		go_CachedSquaredMean		: register(u2);
RWTexture2D<SVGF::TsppSquaredMeanRayHitDistanceFormat>	go_ReprojectedCachedValues	: register(u3);

float4 BilateralResampleWeights(
		float	targetDepth,
		float3	targetNormal,
		float4	sampleDepths,
		float3	sampleNormals[4],
		float2	targetOffset,
		uint2	targetIndex,
		int2	cacheIndices[4],
		float2	ddxy) {
	bool4 isWithinBounds = bool4(
		IsWithinBounds(cacheIndices[0], gTextureDim),
		IsWithinBounds(cacheIndices[1], gTextureDim),
		IsWithinBounds(cacheIndices[2], gTextureDim),
		IsWithinBounds(cacheIndices[3], gTextureDim)
	);

	CrossBilateral::BilinearDepthNormal::Parameters params;
	params.Depth.Sigma = cb_Reproject.DepthSigma;
	params.Depth.WeightCutoff = 0.5;
	params.Depth.NumMantissaBits = cb_Reproject.DepthNumMantissaBits;
	params.Normal.Sigma = 1.1; // Bump the sigma a bit to add tolerance for slight geometry misalignments and/or format precision limitations.
	params.Normal.SigmaExponent = 32;

	float4 bilinearDepthNormalWeights;

	bilinearDepthNormalWeights = CrossBilateral::BilinearDepthNormal::GetWeights(
		targetDepth,
		targetNormal,
		targetOffset,
		ddxy,
		sampleDepths,
		sampleNormals,
		params
	);

	float4 weights = isWithinBounds * bilinearDepthNormalWeights;

	return weights;
}

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	if (!IsWithinBounds(DTid, gTextureDim)) return;

	float2 tex = (DTid + 0.5) * gInvTextureDim;

	float3 reprojNormal;
	float reprojDepth;
	DecodeNormalDepth(gi_ReprojectedNormalDepth[DTid], reprojNormal, reprojDepth);

	float2 velocity = gi_Velocity.SampleLevel(gsamLinearClamp, tex, 0);

	if (reprojDepth == GBuffer::InvalidNormDepthValue || velocity.x > 100) {
		go_CachedTspp[DTid] = 0;
		return;
	}

	float2 cacheTex = tex - velocity;

	// Find the nearest integer index samller than the texture position.
	// The floor() ensures the that value sign is taken into consideration.
	int2 topLeftCacheIndex = floor(cacheTex * gTextureDim - 0.5);
	float2 adjustedCacheTex = (topLeftCacheIndex + 0.5) * gInvTextureDim;

	float2 cachePixelOffset = cacheTex * gTextureDim - 0.5 - topLeftCacheIndex;

	const int2 srcIndexOffsets[4] = { {0,0}, {1,0}, {0,1}, {1,1} };

	int2 cacheIndices[4] = {
		topLeftCacheIndex + srcIndexOffsets[0],
		topLeftCacheIndex + srcIndexOffsets[1],
		topLeftCacheIndex + srcIndexOffsets[2],
		topLeftCacheIndex + srcIndexOffsets[3]
	};

	float3 cacheNormals[4];
	float4 cacheDepths = 0;
	{
		uint4 packed = gi_CachedNormalDepth.GatherRed(gsamPointClamp, adjustedCacheTex).wzxy;
		[unroll]
		for (int i = 0; i < 4; ++i) {
			DecodeNormalDepth(packed[i], cacheNormals[i], cacheDepths[i]);
		}
	}

	float2 ddxy = gi_DepthPartialDerivative.SampleLevel(gsamPointClamp, tex, 0);

	float4 weights = BilateralResampleWeights(reprojDepth, reprojNormal, cacheDepths, cacheNormals, cachePixelOffset, DTid, cacheIndices, ddxy);

	uint2 size;
	gi_CachedValue.GetDimensions(size.x, size.y);

	float dx = 1.0 / size.x;
	float dy = 1.0 / size.y;

	// Invalidate weights for invalid values in the cache.
	float4 vCacheValues[4];
	vCacheValues[0] = gi_CachedValue.SampleLevel(gsamPointClamp, adjustedCacheTex,					0);
	vCacheValues[1] = gi_CachedValue.SampleLevel(gsamPointClamp, adjustedCacheTex + float2(dx, 0),	0);
	vCacheValues[2] = gi_CachedValue.SampleLevel(gsamPointClamp, adjustedCacheTex + float2(0, dy),	0);
	vCacheValues[3] = gi_CachedValue.SampleLevel(gsamPointClamp, adjustedCacheTex + float2(dx, dy),	0);

	if (vCacheValues[0].a > 0) weights.x = 0;
	if (vCacheValues[1].a > 0) weights.y = 0;
	if (vCacheValues[2].a > 0) weights.z = 0;
	if (vCacheValues[3].a > 0) weights.w = 0;

	float weightSum = dot(1, weights);

	float4 cachedValue = 0;
	float4 cachedValueSquaredMean = 0;
	float cachedRayHitDist = 0;

	uint tspp;
	if (weightSum > 0.001) {
		uint4 vCachedTspp = gi_CachedTspp.GatherRed(gsamPointClamp, adjustedCacheTex).wzxy;
		// Enforce tspp of at least 1 for reprojection for valid values.
		// This is because the denoiser will fill in invalid values with filtered 
		// ones if it can. But it doesn't increase tspp.
		vCachedTspp = max(1, vCachedTspp);

		float4 nWeights = weights / weightSum; // Normalize the weights.

		// Scale the tspp by the total weight. This is to keep the tspp low for 
		// total contributions that have very low reprojection weight. While its preferred to get 
		// a weighted value even for reprojections that have low weights but still
		// satisfy consistency tests, the tspp needs to be kept small so that the Target calculated values
		// are quickly filled in over a few frames. Otherwise, bad estimates from reprojections,
		// such as on disocclussions of surfaces on rotation, are kept around long enough to create 
		// visible streaks that fade away very slow.
		// Example: rotating camera around dragon's nose up close. 
		const float TsppScale = 1;

		float cachedTspp = TsppScale * dot(nWeights, vCachedTspp);
		tspp = round(cachedTspp);

		if (tspp > 0) {
			{
				[unroll]
				for (int i = 0; i < 4; ++i)
					cachedValue += nWeights[i] * vCacheValues[i];
			}

			float4 vCachedValueSquaredMeans[4];
			vCachedValueSquaredMeans[0] = gi_CachedValueSquaredMean.SampleLevel(gsamPointClamp, adjustedCacheTex,					0);
			vCachedValueSquaredMeans[1] = gi_CachedValueSquaredMean.SampleLevel(gsamPointClamp, adjustedCacheTex + float2(dx,  0),	0);
			vCachedValueSquaredMeans[2] = gi_CachedValueSquaredMean.SampleLevel(gsamPointClamp, adjustedCacheTex + float2(0,  dy),	0);
			vCachedValueSquaredMeans[3] = gi_CachedValueSquaredMean.SampleLevel(gsamPointClamp, adjustedCacheTex + float2(dx, dy),	0);
			{
				[unroll]
				for (int i = 0; i < 4; ++i)
					cachedValueSquaredMean += nWeights[i] * vCachedValueSquaredMeans[i];
			}

			float4 vCachedRayHitDist = gi_CachedRayHitDistance.GatherRed(gsamPointClamp, adjustedCacheTex).wzxy;
			cachedRayHitDist = dot(nWeights, vCachedRayHitDist);
		}
	}
	else {
		// No valid values can be retrieved from the cache.
		// TODO: try a greater cache footprint to find useful samples,
		//   For example a 3x3 pixel cache footprint or use lower mip cache input.
		tspp = 0;
	}

	go_CachedTspp[DTid] = tspp;
	go_CachedValue[DTid] = cachedValue;
	go_CachedSquaredMean[DTid] = cachedValueSquaredMean;
	go_ReprojectedCachedValues[DTid] = uint4(tspp, f32tof16(float3(cachedRayHitDist, 0, 0)));
}

#endif // __TEMPORALSUPERSAMPLINGREVERSEREPROJECTCS_HLSL__
