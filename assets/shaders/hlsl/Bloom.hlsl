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
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	const float3 color = gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC, 0).rgb;
	const float3 bloom = gi_Bloom.SampleLevel(gsamLinearClamp, pin.TexC, 0).rgb;
	return float4(color + bloom, 1.f);
}

#endif // __BLOOM_HLSL__