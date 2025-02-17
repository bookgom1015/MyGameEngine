#ifndef __COMPUTEGAUSSIANBLUR_HLSL__
#define __COMPUTEGAUSSIANBLUR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Blur> cb_Blur		: register(b0);

BlurFilter_CS_Default_RootConstants(b1)

Texture2D<GBuffer::NormalMapFormat>	gi_Normal	: register(t0);
Texture2D<GBuffer::DepthMapFormat>	gi_Depth	: register(t1);

Texture2D<float>					gi_Input	: register(t2);
RWTexture2D<float>					go_Output	: register(u0);

#define CacheSize (BlurFilter::CS::ThreadGroup::Default::Size + 2 * BlurFilter::CS::MaxBlurRadius)
groupshared float gCache[CacheSize];

[numthreads(BlurFilter::CS::ThreadGroup::Default::Size, 1, 1)]
void HorzBlurCS(uint3 groupThreadID : SV_GroupThreadID, uint3 dispatchThreadID : SV_DispatchThreadID) {
	// unpack into float array.
	const float blurWeights[12] = {
		cb_Blur.BlurWeights[0].x, cb_Blur.BlurWeights[0].y, cb_Blur.BlurWeights[0].z, cb_Blur.BlurWeights[0].w,
		cb_Blur.BlurWeights[1].x, cb_Blur.BlurWeights[1].y, cb_Blur.BlurWeights[1].z, cb_Blur.BlurWeights[1].w,
		cb_Blur.BlurWeights[2].x, cb_Blur.BlurWeights[2].y, cb_Blur.BlurWeights[2].z, cb_Blur.BlurWeights[2].w,
	};

	uint2 length;
	gi_Input.GetDimensions(length.x, length.y);

	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
	if (groupThreadID.x < cb_Blur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		const int x = max(dispatchThreadID.x - cb_Blur.BlurRadius, 0);
		gCache[groupThreadID.x] = gi_Input[int2(x, dispatchThreadID.y)];
	}

	if (groupThreadID.x >= BlurFilter::CS::ThreadGroup::Default::Size - cb_Blur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		const int x = min(dispatchThreadID.x + cb_Blur.BlurRadius, length.x - 1);
		gCache[groupThreadID.x + 2.f * cb_Blur.BlurRadius] = gi_Input[int2(x, dispatchThreadID.y)].r;
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.x + cb_Blur.BlurRadius] = gi_Input[min(dispatchThreadID.xy, length.xy - 1)].r;

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//
	float blurColor = gCache[groupThreadID.x + cb_Blur.BlurRadius] * blurWeights[cb_Blur.BlurRadius];
	float totalWeight = blurWeights[cb_Blur.BlurRadius];

	const float2 centerTex = float2((dispatchThreadID.x + 0.5f) / (float)gDimension.x, (dispatchThreadID.y + 0.5f) / (float)gDimension.y);
	const float3 centerNormal = normalize(gi_Normal.SampleLevel(gsamLinearClamp, centerTex, 0).rgb);
	const float centerDepth = gi_Depth.SampleLevel(gsamLinearClamp, centerTex, 0);

	const float dx = 1.f / gDimension.x;

	for (int i = -cb_Blur.BlurRadius; i <= cb_Blur.BlurRadius; i++) {
		if (i == 0) continue;

		const float2 neighborTex = float2(centerTex.x + dx * i, centerTex.y);
		const float3 neighborNormal = normalize(gi_Normal.SampleLevel(gsamLinearClamp, neighborTex, 0).rgb);
		const float neighborDepth = gi_Depth.SampleLevel(gsamLinearClamp, neighborTex, 0);

		if (dot(neighborNormal, centerNormal) >= 0.95f && abs(neighborDepth - centerDepth) <= 0.01f) {
			const int k = groupThreadID.x + cb_Blur.BlurRadius + i;

			blurColor += gCache[k] * blurWeights[cb_Blur.BlurRadius + i];
			totalWeight += blurWeights[cb_Blur.BlurRadius + i];
		}
	}

	go_Output[dispatchThreadID.xy] = blurColor / totalWeight;
}

[numthreads(1, BlurFilter::CS::ThreadGroup::Default::Size, 1)]
void VertBlurCS(uint3 groupThreadID : SV_GroupThreadID, uint3 dispatchThreadID : SV_DispatchThreadID) {
	// unpack into float array.
	float blurWeights[12] = {
		cb_Blur.BlurWeights[0].x, cb_Blur.BlurWeights[0].y, cb_Blur.BlurWeights[0].z, cb_Blur.BlurWeights[0].w,
		cb_Blur.BlurWeights[1].x, cb_Blur.BlurWeights[1].y, cb_Blur.BlurWeights[1].z, cb_Blur.BlurWeights[1].w,
		cb_Blur.BlurWeights[2].x, cb_Blur.BlurWeights[2].y, cb_Blur.BlurWeights[2].z, cb_Blur.BlurWeights[2].w,
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
	if (groupThreadID.y < cb_Blur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		const int y = max(dispatchThreadID.y - cb_Blur.BlurRadius, 0);
		gCache[groupThreadID.y] = gi_Input[int2(dispatchThreadID.x, y)].r;
	}

	if (groupThreadID.y >= BlurFilter::CS::ThreadGroup::Default::Size - cb_Blur.BlurRadius) {
		// Clamp out of bound samples that occur at image borders.
		const int y = min(dispatchThreadID.y + cb_Blur.BlurRadius, length.y - 1);
		gCache[groupThreadID.y + 2 * cb_Blur.BlurRadius] = gi_Input[int2(dispatchThreadID.x, y)].r;
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.y + cb_Blur.BlurRadius] = gi_Input[min(dispatchThreadID.xy, length.xy - 1)].r;

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//
	float blurColor = gCache[groupThreadID.y + cb_Blur.BlurRadius] * blurWeights[cb_Blur.BlurRadius];
	float totalWeight = blurWeights[cb_Blur.BlurRadius];

	const float2 centerTex = float2((dispatchThreadID.x + 0.5f) / (float)gDimension.x, (dispatchThreadID.y + 0.5f) / (float)gDimension.y);
	const float3 centerNormal = normalize(gi_Normal.SampleLevel(gsamLinearClamp, centerTex, 0).rgb);
	const float centerDepth = gi_Depth.SampleLevel(gsamLinearClamp, centerTex, 0);

	const float dy = 1.f / gDimension.y;

	for (int i = -cb_Blur.BlurRadius; i <= cb_Blur.BlurRadius; i++) {
		if (i == 0) continue;

		const float2 neighborTex = float2(centerTex.x, centerTex.y + dy * i);
		const float3 neighborNormal = normalize(gi_Normal.SampleLevel(gsamLinearClamp, neighborTex, 0).rgb);
		const float neighborDepth = gi_Depth.SampleLevel(gsamLinearClamp, neighborTex, 0);

		if (dot(neighborNormal, centerNormal) >= 0.95f && abs(neighborDepth - centerDepth) <= 0.01f) {
			const int k = groupThreadID.y + cb_Blur.BlurRadius + i;

			blurColor += gCache[k] * blurWeights[cb_Blur.BlurRadius + i];
			totalWeight += blurWeights[cb_Blur.BlurRadius + i];
		}
	}

	go_Output[dispatchThreadID.xy] = blurColor / totalWeight;
}

#endif // __COMPUTEGAUSSIANBLUR_HLSL__