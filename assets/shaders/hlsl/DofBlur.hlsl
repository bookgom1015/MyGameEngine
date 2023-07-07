#ifndef __DOFBLUR_HLSL__
#define __DOFBLUR_HLSL__

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

Texture2D gInputMap		: register(t0);
Texture2D gCocMap		: register(t1);

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
	gInputMap.GetDimensions(width, height);

	float dx = 1.0f / width;
	float dy = 1.0f / height;

	float2 texOffset;
	if (gHorizontalBlur) texOffset = float2(dx, 0.0f);
	else texOffset = float2(0.0f, dy);

	float totalWeight = blurWeights[gBlurRadius];
	float3 color = totalWeight * gInputMap.Sample(gsamPointClamp, pin.TexC).rgb;

	float centerCoc = abs(gCocMap.Sample(gsamPointClamp, pin.TexC).r);

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i) {
		if (i == 0) continue;

		float2 tex = pin.TexC + i * texOffset;

		float coc = abs(min(gCocMap.Sample(gsamPointClamp, tex).r, 0.0f));
		float weight = blurWeights[i + gBlurRadius] * centerCoc;

		color += weight * gInputMap.Sample(gsamPointClamp, tex).rgb;
		totalWeight += weight;
	}
	color /= totalWeight;

	return float4(color, 1.0f);
}

#endif // __DOFBLUR_HLSL__