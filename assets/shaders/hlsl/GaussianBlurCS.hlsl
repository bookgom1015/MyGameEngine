#ifndef __COMPUTEGAUSSIANBLUR_HLSL__
#define __COMPUTEGAUSSIANBLUR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<BlurConstants> cbBlur : register(b0);

cbuffer cbRootConstants : register(b1) {
	uint2 gDimension;
}

Texture2D<float3>	gi_Normal	: register(t0);
Texture2D<float>	gi_Depth	: register(t1);

Texture2D<float>	gi_Input	: register(t2);
RWTexture2D<float>	go_Output	: register(u0);

#define CacheSize (BlurFilterCS ::ThreadGroup::Size + 2 * BlurFilterCS ::MaxBlurRadius)
groupshared float gCache[CacheSize];

#define Deadline 0.1f

[numthreads(BlurFilterCS::ThreadGroup::Size, 1, 1)]
void HorzBlurCS(uint3 groupThreadID : SV_GroupThreadID, uint3 dispatchThreadID : SV_DispatchThreadID) {
	// unpack into float array.
	float blurWeights[12] = {
		cbBlur.BlurWeights[0].x, cbBlur.BlurWeights[0].y, cbBlur.BlurWeights[0].z, cbBlur.BlurWeights[0].w,
		cbBlur.BlurWeights[1].x, cbBlur.BlurWeights[1].y, cbBlur.BlurWeights[1].z, cbBlur.BlurWeights[1].w,
		cbBlur.BlurWeights[2].x, cbBlur.BlurWeights[2].y, cbBlur.BlurWeights[2].z, cbBlur.BlurWeights[2].w,
	};

	uint2 length;
	gi_Input.GetDimensions(length.x, length.y);

	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
	if (groupThreadID.x < cbBlur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		int x = max(dispatchThreadID.x - cbBlur.BlurRadius, 0);
		gCache[groupThreadID.x] = gi_Input[int2(x, dispatchThreadID.y)];
	}

	if (groupThreadID.x >= BlurFilterCS::ThreadGroup::Size - cbBlur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		int x = min(dispatchThreadID.x + cbBlur.BlurRadius, length.x - 1);
		gCache[groupThreadID.x + 2 * cbBlur.BlurRadius] = gi_Input[int2(x, dispatchThreadID.y)].r;
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.x + cbBlur.BlurRadius] = gi_Input[min(dispatchThreadID.xy, length.xy - 1)].r;

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//
	float blurColor = gCache[groupThreadID.x + cbBlur.BlurRadius] * blurWeights[cbBlur.BlurRadius];
	float totalWeight = blurWeights[cbBlur.BlurRadius];

	float2 centerTex = float2((dispatchThreadID.x + 0.5f) / (float)gDimension.x, (dispatchThreadID.y + 0.5f) / (float)gDimension.y);
	float3 centerNormal = gi_Normal.SampleLevel(gsamLinearClamp, centerTex, 0);
	float centerDepth = gi_Depth.SampleLevel(gsamLinearClamp, centerTex, 0);

	float dx = 1.0f / gDimension.x;

	for (int i = -cbBlur.BlurRadius; i <= cbBlur.BlurRadius; i++) {
		if (i == 0) continue;

		float2 neighborTex = float2(centerTex.x + dx * i, centerTex.y);
		float3 neighborNormal = gi_Normal.SampleLevel(gsamLinearClamp, neighborTex, 0);
		float neighborDepth = gi_Depth.SampleLevel(gsamLinearClamp, neighborTex, 0);

		if (dot(neighborNormal, centerNormal) >= 0.95f && abs(neighborDepth - centerDepth) <= 0.01f) {
			int k = groupThreadID.x + cbBlur.BlurRadius + i;

			blurColor += gCache[k] * blurWeights[cbBlur.BlurRadius + i];
			totalWeight += blurWeights[cbBlur.BlurRadius + i];
		}
	}

	go_Output[dispatchThreadID.xy] = blurColor / totalWeight;
}

[numthreads(1, BlurFilterCS::ThreadGroup::Size, 1)]
void VertBlurCS(uint3 groupThreadID : SV_GroupThreadID, uint3 dispatchThreadID : SV_DispatchThreadID) {
	// unpack into float array.
	float blurWeights[12] = {
		cbBlur.BlurWeights[0].x, cbBlur.BlurWeights[0].y, cbBlur.BlurWeights[0].z, cbBlur.BlurWeights[0].w,
		cbBlur.BlurWeights[1].x, cbBlur.BlurWeights[1].y, cbBlur.BlurWeights[1].z, cbBlur.BlurWeights[1].w,
		cbBlur.BlurWeights[2].x, cbBlur.BlurWeights[2].y, cbBlur.BlurWeights[2].z, cbBlur.BlurWeights[2].w,
	};

	//
	// Fill local thread storage to reduce bandwidth.  To blur 
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//

	uint2 length;
	gi_Input.GetDimensions(length.x, length.y);

	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
	if (groupThreadID.y < cbBlur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		int y = max(dispatchThreadID.y - cbBlur.BlurRadius, 0);
		gCache[groupThreadID.y] = gi_Input[int2(dispatchThreadID.x, y)].r;
	}

	if (groupThreadID.y >= BlurFilterCS::ThreadGroup::Size - cbBlur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		int y = min(dispatchThreadID.y + cbBlur.BlurRadius, length.y - 1);
		gCache[groupThreadID.y + 2 * cbBlur.BlurRadius] = gi_Input[int2(dispatchThreadID.x, y)].r;
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.y + cbBlur.BlurRadius] = gi_Input[min(dispatchThreadID.xy, length.xy - 1)].r;

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//
	float blurColor = gCache[groupThreadID.y + cbBlur.BlurRadius] * blurWeights[cbBlur.BlurRadius];
	float totalWeight = blurWeights[cbBlur.BlurRadius];

	float2 centerTex = float2((dispatchThreadID.x + 0.5f) / (float)gDimension.x, (dispatchThreadID.y + 0.5f) / (float)gDimension.y);
	float3 centerNormal = gi_Normal.SampleLevel(gsamLinearClamp, centerTex, 0);
	float centerDepth = gi_Depth.SampleLevel(gsamLinearClamp, centerTex, 0);

	float dy = 1.0f / gDimension.y;

	for (int i = -cbBlur.BlurRadius; i <= cbBlur.BlurRadius; i++) {
		if (i == 0) continue;

		float2 neighborTex = float2(centerTex.x, centerTex.y + dy * i);
		float3 neighborNormal = gi_Normal.SampleLevel(gsamLinearClamp, neighborTex, 0);
		float neighborDepth = gi_Depth.SampleLevel(gsamLinearClamp, neighborTex, 0);

		if (dot(neighborNormal, centerNormal) >= 0.95f && abs(neighborDepth - centerDepth) <= 0.01f) {
			int k = groupThreadID.y + cbBlur.BlurRadius + i;

			blurColor += gCache[k] * blurWeights[cbBlur.BlurRadius + i];
			totalWeight += blurWeights[cbBlur.BlurRadius + i];
		}
	}

	go_Output[dispatchThreadID.xy] = blurColor / totalWeight;
}

#endif // __COMPUTEGAUSSIANBLUR_HLSL__