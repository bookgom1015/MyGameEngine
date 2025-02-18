#ifndef __BLURDOF_HLSL__
#define __BLURDOF_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Blur>				cb_Blur			: register(b0);

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
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	const float blurWeights[12] = {
		cb_Blur.BlurWeights[0].x, cb_Blur.BlurWeights[0].y, cb_Blur.BlurWeights[0].z, cb_Blur.BlurWeights[0].w,
		cb_Blur.BlurWeights[1].x, cb_Blur.BlurWeights[1].y, cb_Blur.BlurWeights[1].z, cb_Blur.BlurWeights[1].w,
		cb_Blur.BlurWeights[2].x, cb_Blur.BlurWeights[2].y, cb_Blur.BlurWeights[2].z, cb_Blur.BlurWeights[2].w,
	};

	uint2 size;
	gi_BackBuffer.GetDimensions(size.x, size.y);

	const float dx = 1.f / size.x;
	const float dy = 1.f / size.y;

	float weightSum = blurWeights[cb_Blur.BlurRadius];
	float3 colorSum = weightSum * gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC, 0).rgb;
	const float centerCoC = abs(gi_CoC.SampleLevel(gsamPointClamp, pin.TexC, 0));

	for (int i = -cb_Blur.BlurRadius; i <= cb_Blur.BlurRadius; ++i) {
		for (int j = -cb_Blur.BlurRadius; j <= cb_Blur.BlurRadius; ++j) {
			if (i == 0 && j == 0) continue;

			const float2 texOffset = float2(dx * i, dy * j);
			const float2 texc = pin.TexC + texOffset;

			const float neighborCoC = abs(gi_CoC.SampleLevel(gsamPointClamp, texc, 0));
			const float weight = blurWeights[i + cb_Blur.BlurRadius] * blurWeights[j + cb_Blur.BlurRadius] * centerCoC * neighborCoC;

			colorSum += weight * gi_BackBuffer.SampleLevel(gsamPointClamp, texc, 0).rgb;
			weightSum += weight;
		}
	}
	colorSum /= weightSum;

	return float4(colorSum, 1.f);
}

#endif // __BLURDOF_HLSL__