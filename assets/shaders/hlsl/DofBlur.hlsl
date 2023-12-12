#ifndef __DOFBLUR_HLSL__
#define __DOFBLUR_HLSL__

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

Texture2D<ToneMapping::IntermediateMapFormat>	gi_BackBuffer	: register(t0);
Texture2D<DepthOfField::CocMapFormat>			gi_Coc			: register(t1);

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
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	// unpack into float array.
	float blurWeights[12] = {
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
	};

	uint2 size;
	gi_BackBuffer.GetDimensions(size.x, size.y);

	float dx = 1.0 / size.x;
	float dy = 1.0 / size.y;

	float totalWeight = blurWeights[gBlurRadius];
	float3 color = totalWeight * gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC).rgb;

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i) {
		for (int j = -gBlurRadius; j <= gBlurRadius; ++j) {
			if (i == 0 && j == 0) continue;

			float2 texOffset = float2(dx * i, dy * j);
			float2 texc = pin.TexC + texOffset;

			float w_c = abs(gi_Coc.Sample(gsamLinearClamp, texc));
			if (w_c <= 0.1) w_c = 0;
			else w_c = 1;

			float weight = blurWeights[i + gBlurRadius] * blurWeights[j + gBlurRadius] * w_c;

			color += weight * gi_BackBuffer.Sample(gsamLinearClamp, texc).rgb;
			totalWeight += weight;
		}
	}
	color /= totalWeight;

	return float4(color, 1);
}

#endif // __DOFBLUR_HLSL__