#ifndef __BLOOM_HLSL__
#define __BLOOM_HLSL__

#include "Common.hlsli"

Texture2D gBackBuffer	: register(t0);
Texture2D gBloomMap		: register(t1);

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
	float3 PosV		: POSITION;
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
	float3 bloom = gBloomMap.Sample(gsamLinearWrap, pin.TexC).rgb;	
	return float4(color + bloom, 1.0f);
}

#endif // __BLOOM_HLSL__