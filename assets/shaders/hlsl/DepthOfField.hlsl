#ifndef __DEPTHOFFIELD_HLSL__
#define __DEPTHOFFIELD_HLSL__

#include "Samplers.hlsli"

#ifndef NUM_SAMPLES
#define NUM_SAMPLES 4
#endif

Texture2D gBackBuffer	: register(t0);
Texture2D gCocMap		: register(t1);

cbuffer cbRootConstants : register(b0) {
	float gBokehRadius;
	float gCocThreshold;
	float gCocDiffThreshold;
	float gHighlightPower;
	float gNumSamples;
};

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

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

float4 PS(VertexOut pin) : SV_Target{
	uint width, height;
	gBackBuffer.GetDimensions(width, height);

	float dx = gBokehRadius / width;
	float dy = gBokehRadius / height;

	float currCoc = gCocMap.Sample(gsamPointClamp, pin.TexC).r;
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
			float coc = gCocMap.Sample(gsamPointClamp, tex).r;
			if (coc > -gCocThreshold && (radius > blurRadius || abs(currCoc - coc) > gCocDiffThreshold)) continue;

			float3 color = gBackBuffer.Sample(gsamPointClamp, tex).rgb;
			float3 powered = pow(color, gHighlightPower);

			finalColor += color * powered;
			total += powered;
		}
	}
	finalColor /= total;

	return float4(finalColor, 1.0f);
}

#endif // __DEPTHOFFIELD_HLSL__