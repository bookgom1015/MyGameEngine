#ifndef __MAPPING_HLSL__
#define __MAPPING_HLSL__

#include "Samplers.hlsli"

Texture2D gInputMap : register(t0);

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

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

float4 PS(VertexOut pin) : SV_Target {
	return float4(gInputMap.Sample(gsamLinearWrap, pin.TexC).rgb, 1.0f);
}

#endif // __MAPPING_HLSL__