#ifndef __BLOOM_HLSL__
#define __BLOOM_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

Texture2D<ToneMapping::IntermediateMapFormat>	gi_BackBuffer	: register(t0);
Texture2D<Bloom::BloomMapFormat>				gi_Bloom		: register(t1);

#include "CoordinatesFittedToScreen.hlsli"

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
	float3 color = gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC).rgb;
	float3 bloom = gi_Bloom.Sample(gsamLinearClamp, pin.TexC).rgb;
	return float4(color + bloom, 1.0f);
}

#endif // __BLOOM_HLSL__