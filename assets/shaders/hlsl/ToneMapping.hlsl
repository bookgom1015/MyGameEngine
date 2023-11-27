#ifndef __TONEMAPPING_HLSL__
#define __TONEMAPPING_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

cbuffer gRootConstants : register(b0) {
	float gExposure;
}

Texture2D<ToneMapping::IntermediateMapFormat> gi_Intermediate : register(t0);

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	float2 pos = float2(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y);

	// Already in homogeneous clip space.
	vout.PosH = float4(pos, 0, 1);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	float2 tex = pin.TexC;

	float3 hdr = gi_Intermediate.SampleLevel(gsamPointClamp, tex, 0).rgb;

#if NON_HDR
	float3 mapped = hdr;
#else

	float3 mapped = (float3)1 - exp(-hdr * gExposure);
#endif

	return float4(mapped, 1);
}

#endif // __TONEMAPPING_HLSL__