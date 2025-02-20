#ifndef __TONEMAPPING_HLSL__
#define __TONEMAPPING_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

ToneMapping_Default_RootConstants(b0)

Texture2D<ToneMapping::IntermediateMapFormat> gi_Intermediate : register(t0);

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	const float2 pos = float2(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y);
	vout.PosH = float4(pos, 0.f, 1.f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	const float2 tex = pin.TexC;
	const float3 hdr = gi_Intermediate.SampleLevel(gsamPointClamp, tex, 0).rgb;

#if NON_HDR
	const float3 mapped = hdr;
#else

	const float3 mapped = (float3)1.f - exp(-hdr * gExposure);
#endif

	return float4(mapped, 1.f);
}

#endif // __TONEMAPPING_HLSL__