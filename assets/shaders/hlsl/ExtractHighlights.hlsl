#ifndef __EXTRACTHIGHLIGHTS_HLSL__
#define __EXTRACTHIGHLIGHTS_HLSL__

#include "Samplers.hlsli"

Texture2D gBackBuffer : register(t0);

cbuffer cbRootConstants : register(b0) {
	float gThreshold;
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
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	float3 color = gBackBuffer.Sample(gsamLinearWrap, pin.TexC).rgb;
	float brightness = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));

	if (brightness > gThreshold) return float4(color, 1.0f);
	else return (float4)0.0f;
}

#endif // __EXTRACTHIGHLIGHTS_HLSL__