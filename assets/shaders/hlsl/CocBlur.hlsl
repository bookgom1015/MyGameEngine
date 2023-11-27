#ifndef __COCBLUR_HLSL__
#define __COCBLUR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
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
	bool	gHorizontalBlur;
};

Texture2D<DepthOfField::CocMapFormat> gi_Coc	: register(t0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	// unpack into float array.
	float blurWeights[12] = {
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
	};

	uint width, height;
	gi_Coc.GetDimensions(width, height);

	float dx = 1.0f / width;
	float dy = 1.0f / height;

	float2 texOffset;
	if (gHorizontalBlur) texOffset = float2(dx, 0.0f);
	else texOffset = float2(0.0f, dy);

	float coc = blurWeights[gBlurRadius] * gi_Coc.Sample(gsamLinearWrap, pin.TexC);
	float totalWeight = blurWeights[gBlurRadius];

	for (float i = -gBlurRadius; i <= gBlurRadius; ++i) {
		if (i == 0) continue;

		float2 tex = pin.TexC + i * texOffset;

		float weight = blurWeights[i + gBlurRadius];

		coc += weight * gi_Coc.Sample(gsamPointWrap, tex, 0.0f);

		totalWeight += weight;
	}

	return (float4)coc / totalWeight;
}

#endif // __COCBLUR_HLSL__