#ifndef __DEBUG_HLSL__
#define __DEBUG_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

cbuffer gRootConstants : register(b0) {
	uint gSampleMask0;
	uint gSampleMask1;
	uint gSampleMask2;
	uint gSampleMask3;
	uint gSampleMask4;
}

Texture2D gi_Debug0	: register(t0);
Texture2D gi_Debug1	: register(t1);
Texture2D gi_Debug2	: register(t2);
Texture2D gi_Debug3	: register(t3);
Texture2D gi_Debug4	: register(t4);

static const float2 gOffsets[DebugShaderParams::MapCount] = {
	float2(0.8f, 0.8f),
	float2(0.8f, 0.4f),
	float2(0.8f, 0.0f),
	float2(0.8f, -0.4f),
	float2(0.8f, -0.8f)
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
	uint   InstID	: INSTID;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;

	vout.TexC = gTexCoords[vid];
	vout.InstID = instanceID;

	// Quad covering screen in NDC space.
	float2 pos = float2(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y) * 0.2f + gOffsets[instanceID];

	// Already in homogeneous clip space.
	vout.PosH = float4(pos, 0.0f, 1.0f);

	return vout;
}

uint GetSampleMask(int index) {
	switch (index) {
	case 0: return gSampleMask0;
	case 1: return gSampleMask1;
	case 2: return gSampleMask2;
	case 3: return gSampleMask3;
	case 4: return gSampleMask4;
	}
	return 0;
}

float4 SampleColor(in Texture2D map, int index, float2 tex) {
	uint mask = GetSampleMask(index);
	switch (mask) {
	case DebugShaderParams::SampleMask::RGB: {
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).rgb;
		return float4(samp, 1);
	}
	case DebugShaderParams::SampleMask::RG: {
		float2 samp = map.SampleLevel(gsamPointClamp, tex, 0).rg;
		return float4(samp, 0, 1);
	}
	case DebugShaderParams::SampleMask::RRR: {
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).rrr;
		return float4(samp, 1);
	}
	case DebugShaderParams::SampleMask::GGG: {
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).ggg;
		return float4(samp, 1);
	}
	case DebugShaderParams::SampleMask::AAA: {
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).aaa;
		return float4(samp, 1);
	}
	}
	return 1;
}

float4 PS(VertexOut pin) : SV_Target {
	switch (pin.InstID) {
	case 0: return SampleColor(gi_Debug0, 0, pin.TexC);
	case 1: return SampleColor(gi_Debug1, 1, pin.TexC);
	case 2: return SampleColor(gi_Debug2, 2, pin.TexC);
	case 3: return SampleColor(gi_Debug3, 3, pin.TexC);
	case 4: return SampleColor(gi_Debug4, 4, pin.TexC);
	}
	return float4(1, 0, 1, 1);
}

#endif // __DEBUG_HLSL__