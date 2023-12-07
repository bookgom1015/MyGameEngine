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
Texture2D<float4>					gi_Input	: register(t2);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH  : SV_POSITION;
	float2 TexC  : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

float4 PS(VertexOut pin) : SV_Target {
	// unpack into float array.
	const float blurWeights[12] = {
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
	};

	uint width, height;
	gi_Input.GetDimensions(width, height);

	const float dx = 1.0f / width;
	const float dy = 1.0f / height;

	float2 texOffset;
	if (gHorizontal) texOffset = float2(dx, 0.0f);
	else texOffset = float2(0.0f, dy);

	// The center value always contributes to the sum.
	float4 color = blurWeights[gBlurRadius] * gi_Input.Sample(gsamLinearClamp, pin.TexC);
	float totalWeight = blurWeights[gBlurRadius];

	float3 centerNormal;
	float centerDepth;
	if (gBilateral) {		
		centerNormal = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);
		centerDepth = NdcDepthToViewDepth(gi_Depth.Sample(gsamDepthMap, pin.TexC));
	}

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)	{
		// We already added in the center weight.
		if (i == 0) continue;

		float2 tex = pin.TexC + i * texOffset;

		if (gBilateral) {
			const float3 neighborNormal = normalize(gi_Normal.Sample(gsamLinearClamp, tex).xyz);
			const float neighborDepth = NdcDepthToViewDepth(gi_Depth.Sample(gsamDepthMap, tex));

			//
			// If the center value and neighbor values differ too much (either in 
			// normal or depth), then we assume we are sampling across a discontinuity.
			// We discard such samples from the blur.
			//
			if (dot(neighborNormal, centerNormal) >= 0.75f && abs(neighborDepth - centerDepth) <= 1.0f) {
				float weight = blurWeights[i + gBlurRadius];

				// Add neighbor pixel to blur.
				color += weight * gi_Input.Sample(gsamLinearClamp, tex);

				totalWeight += weight;
			}
		}
		else {
			const float weight = blurWeights[i + gBlurRadius];

			color += weight * gi_Input.Sample(gsamLinearClamp, tex);

			totalWeight += weight;
		}
	}

	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}

#endif // __GAUSSIANBLUR_HLSL__
