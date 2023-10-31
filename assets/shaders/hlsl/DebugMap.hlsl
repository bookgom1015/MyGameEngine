#ifndef __DEBUGMAP_HLSL__
#define __DEBUGMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

ConstantBuffer<DebugMapConstantBuffer> cbDebug : register(b0);

cbuffer gRootConstants : register(b1) {
	uint gSampleMask0;
	uint gSampleMask1;
	uint gSampleMask2;
	uint gSampleMask3;
	uint gSampleMask4;
}

Texture2D gi_Debug0 : register(t0);
Texture2D gi_Debug1 : register(t1);
Texture2D gi_Debug2 : register(t2);
Texture2D gi_Debug3 : register(t3);
Texture2D gi_Debug4 : register(t4);

Texture2D<uint> gi_Debug0_uint : register(t0, space1);
Texture2D<uint> gi_Debug1_uint : register(t1, space1);
Texture2D<uint> gi_Debug2_uint : register(t2, space1);
Texture2D<uint> gi_Debug3_uint : register(t3, space1);
Texture2D<uint> gi_Debug4_uint : register(t4, space1);

static const float2 gOffsets[DebugMap::MapSize] = {
	float2(0.8,  0.8),
	float2(0.8,  0.4),
	float2(0.8,  0.0),
	float2(0.8, -0.4),
	float2(0.8, -0.8)
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
	float2 pos = float2(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y) * 0.2 + gOffsets[instanceID];

	// Already in homogeneous clip space.
	vout.PosH = float4(pos, 0, 1);

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

Texture2D TextureObject(int index) {
	switch (index) {
	case 0: return gi_Debug0;
	case 1: return gi_Debug1;
	case 2: return gi_Debug2;
	case 3: return gi_Debug3;
	case 4: return gi_Debug4;
	}
	return gi_Debug0;
}

Texture2D<uint> TextureObject_uint(int index) {
	switch (index) {
	case 0: return gi_Debug0_uint;
	case 1: return gi_Debug1_uint;
	case 2: return gi_Debug2_uint;
	case 3: return gi_Debug3_uint;
	case 4: return gi_Debug4_uint;
	}
	return gi_Debug0_uint;
}

float4 SampleColor(int index, float2 tex) {
	uint mask = GetSampleMask(index);
	switch (mask) {
	case DebugMap::SampleMask::RGB: {
		Texture2D map = TextureObject(index);
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).rgb;
		return float4(samp, 1);
	}
	case DebugMap::SampleMask::RG: {
		Texture2D map = TextureObject(index);
		float2 samp = map.SampleLevel(gsamPointClamp, tex, 0).rg;
		return float4(samp, 0, 1);
	}
	case DebugMap::SampleMask::RRR: {
		Texture2D map = TextureObject(index);
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).rrr;
		return float4(samp, 1);
	}
	case DebugMap::SampleMask::GGG: {
		Texture2D map = TextureObject(index);
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).ggg;
		return float4(samp, 1);
	}
	case DebugMap::SampleMask::AAA: {
		Texture2D map = TextureObject(index);
		float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).aaa;
		return float4(samp, 1);
	}
	case DebugMap::SampleMask::FLOAT: {
		Texture2D map = TextureObject(index);
		float samp = map.SampleLevel(gsamPointClamp, tex, 0).x;
		float finalColor = samp / cbDebug.SampleDescs[index].Denominator;
		return finalColor >= 0 ? lerp(cbDebug.SampleDescs[index].MinColor, cbDebug.SampleDescs[index].MaxColor, finalColor) : 1;
	}
	case DebugMap::SampleMask::UINT: {
		Texture2D<uint> map = TextureObject_uint(index);
		uint samp = map.Load(int3(tex.x * 800, tex.y * 600, 0)).x;
		float finalColor = min(1, samp / cbDebug.SampleDescs[index].Denominator);
		return lerp(cbDebug.SampleDescs[index].MinColor, cbDebug.SampleDescs[index].MaxColor, finalColor);
	}
	}
	return 1;
}

float4 PS(VertexOut pin) : SV_Target {
	switch (pin.InstID) {
	case 0: return SampleColor(0, pin.TexC);
	case 1: return SampleColor(1, pin.TexC);
	case 2: return SampleColor(2, pin.TexC);
	case 3: return SampleColor(3, pin.TexC);
	case 4: return SampleColor(4, pin.TexC);
	}
	return float4(1, 0, 1, 1);
}

#endif // __DEBUGMAP_HLSL__