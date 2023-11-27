#ifndef __DEPTHOFFIELD_HLSL__
#define __DEPTHOFFIELD_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

#ifndef NUM_SAMPLES
#define NUM_SAMPLES 4
#endif

Texture2D<ToneMapping::IntermediateMapFormat>	gi_BackBuffer	: register(t0);
Texture2D<DepthOfField::CocMapFormat>			gi_Coc			: register(t1);

cbuffer cbRootConstants : register(b0) {
	float gBokehRadius;
	float gCocThreshold;
	float gCocDiffThreshold;
	float gHighlightPower;
	float gNumSamples;
};

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
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
	uint width, height;
	gi_BackBuffer.GetDimensions(width, height);

	float dx = gBokehRadius / width;
	float dy = gBokehRadius / height;

	float currCoc = gi_Coc.Sample(gsamPointClamp, pin.TexC);
	int blurRadius = gNumSamples * abs(currCoc);

	float3 finalColor = (float3)0.0f;
	float3 total = (float3)0.0f;

	[loop]
	for (int i = -gNumSamples; i <= gNumSamples; ++i) {
		[loop]
		for (int j = -gNumSamples; j <= gNumSamples; ++j) {
			float radius = sqrt(i * i + j * j);
			if (radius > gNumSamples) continue;

			float2 tex = pin.TexC + float2(i * dx, j * dy);
			float coc = gi_Coc.Sample(gsamPointClamp, tex);
			if (coc > -gCocThreshold && (radius > blurRadius || abs(currCoc - coc) > gCocDiffThreshold)) continue;

			float3 color = gi_BackBuffer.Sample(gsamPointClamp, tex).rgb;
			float3 powered = pow(color, gHighlightPower);

			finalColor += color * powered;
			total += powered;
		}
	}
	finalColor /= total;

	return float4(finalColor, 1.0f);
}

#endif // __DEPTHOFFIELD_HLSL__