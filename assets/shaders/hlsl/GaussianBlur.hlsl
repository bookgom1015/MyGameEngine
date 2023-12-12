#ifndef __GAUSSIANBLUR_HLSL__
#define __GAUSSIANBLUR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

cbuffer cbBlur : register(b0) {
	float4x4	gProj;
	float4		gBlurWeights[3];
	float		gBlurRadius;
	float		gConstantPad0;
	float		gConstantPad1;
	float		gConstantPad2;
}

cbuffer cbRootConstants : register(b1) {
	bool gHorizontal;
	bool gBilateral;
};

Texture2D<GBuffer::NormalMapFormat>	gi_Normal	: register(t0);
Texture2D<GBuffer::DepthMapFormat>	gi_Depth	: register(t1);

Texture2D<float4>					gi_Input_F4	: register(t2);
Texture2D<float3>					gi_Input_F3	: register(t3);
Texture2D<float2>					gi_Input_F2	: register(t4);
Texture2D<float>					gi_Input_F1	: register(t5);

#if FT_F4
typedef float4 PixelOut;
#elif FT_F3
typedef float3 PixelOut;
#elif FT_F2
typedef float2 PixelOut;
#else
typedef float PixelOut;
#endif

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH  : SV_POSITION;
	float2 TexC  : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

PixelOut PS(VertexOut pin) : SV_Target {
	// unpack into float array.
	const float blurWeights[12] = {
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
	};

	uint2 size;
#if FT_F4
	gi_Input_F4.GetDimensions(size.x, size.y);
#elif FT_F3
	gi_Input_F3.GetDimensions(size.x, size.y);
#elif FT_F2
	gi_Input_F2.GetDimensions(size.x, size.y);
#else
	gi_Input_F1.GetDimensions(size.x, size.y);
#endif

	const float dx = 1.0 / size.x;
	const float dy = 1.0 / size.y;

	float2 texOffset;
	if (gHorizontal) texOffset = float2(dx, 0);
	else texOffset = float2(0, dy);

	// The center value always contributes to the sum.
#if FT_F4
	float4 color = blurWeights[gBlurRadius] * gi_Input_F4.Sample(gsamLinearClamp, pin.TexC);
#elif FT_F3
	float3 color = blurWeights[gBlurRadius] * gi_Input_F3.Sample(gsamLinearClamp, pin.TexC);
#elif FT_F2
	float2 color = blurWeights[gBlurRadius] * gi_Input_F2.Sample(gsamLinearClamp, pin.TexC);
#else
	float color = blurWeights[gBlurRadius] * gi_Input_F1.Sample(gsamLinearClamp, pin.TexC);
#endif
	float totalWeight = blurWeights[gBlurRadius];

	float3 centerNormal;
	float centerDepth;
	if (gBilateral) {		
		centerNormal = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);
		centerDepth = NdcDepthToViewDepth(gi_Depth.Sample(gsamDepthMap, pin.TexC), gProj);
	}

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)	{
		// We already added in the center weight.
		if (i == 0) continue;

		float2 tex = pin.TexC + i * texOffset;

		if (gBilateral) {
			const float3 neighborNormal = normalize(gi_Normal.Sample(gsamLinearClamp, tex).xyz);
			const float neighborDepth = NdcDepthToViewDepth(gi_Depth.Sample(gsamDepthMap, tex), gProj);

			//
			// If the center value and neighbor values differ too much (either in 
			// normal or depth), then we assume we are sampling across a discontinuity.
			// We discard such samples from the blur.
			//
			if (dot(neighborNormal, centerNormal) >= 0.75 && abs(neighborDepth - centerDepth) <= 1) {
				float weight = blurWeights[i + gBlurRadius];

				// Add neighbor pixel to blur.
#if FT_F4
				color += weight * gi_Input_F4.Sample(gsamLinearClamp, tex);
#elif FT_F3
				color += weight * gi_Input_F3.Sample(gsamLinearClamp, tex);
#elif FT_F2
				color += weight * gi_Input_F2.Sample(gsamLinearClamp, tex);
#else
				color += weight * gi_Input_F1.Sample(gsamLinearClamp, tex);
#endif

				totalWeight += weight;
			}
		}
		else {
			const float weight = blurWeights[i + gBlurRadius];

#if FT_F4
			color += weight * gi_Input_F4.Sample(gsamLinearClamp, tex);
#elif FT_F3
			color += weight * gi_Input_F3.Sample(gsamLinearClamp, tex);
#elif FT_F2
			color += weight * gi_Input_F2.Sample(gsamLinearClamp, tex);
#else
			color += weight * gi_Input_F1.Sample(gsamLinearClamp, tex);
#endif

			totalWeight += weight;
		}
	}

	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}

#endif // __GAUSSIANBLUR_HLSL__
