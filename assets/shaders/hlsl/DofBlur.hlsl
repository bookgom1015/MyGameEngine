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
Texture2D<DepthOfField::CoCMapFormat>			gi_CoC			: register(t1);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	const float blurWeights[12] = {
		gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
		gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
		gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
	};

	uint2 size;
	gi_BackBuffer.GetDimensions(size.x, size.y);

	const float dx = 1.0 / size.x;
	const float dy = 1.0 / size.y;

	float weightSum = blurWeights[gBlurRadius];
	float3 colorSum = weightSum * gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC).rgb;
	const float centerCoC = abs(gi_CoC.Sample(gsamPointClamp, pin.TexC));

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i) {
		for (int j = -gBlurRadius; j <= gBlurRadius; ++j) {
			if (i == 0 && j == 0) continue;

			const float2 texOffset = float2(dx * i, dy * j);
			const float2 texc = pin.TexC + texOffset;

			const float neighborCoC = abs(gi_CoC.Sample(gsamPointClamp, texc));
			const float weight = blurWeights[i + gBlurRadius] * blurWeights[j + gBlurRadius] * centerCoC * neighborCoC;

			colorSum += weight * gi_BackBuffer.Sample(gsamPointClamp, texc).rgb;
			weightSum += weight;
		}
	}
	colorSum /= weightSum;

	return float4(colorSum, 1);
}

#endif // __DOFBLUR_HLSL__