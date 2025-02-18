#ifndef __DEBUGTEXMAP_HLSL__
#define __DEBUGTEXMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Debug> cb_Debug : register(b0);

Debug_DebugTexMap_RootConstants(b1)

#include "CoordinatesFittedToScreen.hlsli"

Texture2D		gi_Debug0		: register(t0);
Texture2D		gi_Debug1		: register(t1);
Texture2D		gi_Debug2		: register(t2);
Texture2D		gi_Debug3		: register(t3);
Texture2D		gi_Debug4		: register(t4);

Texture2D<uint> gi_Debug0_uint	: register(t0, space1);
Texture2D<uint> gi_Debug1_uint	: register(t1, space1);
Texture2D<uint> gi_Debug2_uint	: register(t2, space1);
Texture2D<uint> gi_Debug3_uint	: register(t3, space1);
Texture2D<uint> gi_Debug4_uint	: register(t4, space1);

static const float2 gOffsets[Debug::MapSize] = {
	float2(0.8f,  0.8f),
	float2(0.8f,  0.4f),
	float2(0.8f,  0.0f),
	float2(0.8f, -0.4f),
	float2(0.8f, -0.8f)
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
	uint   InstID	: INSTID;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];
	vout.InstID = instanceID;

	const float2 pos = float2(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y) * 0.2f + gOffsets[instanceID];
	vout.PosH = float4(pos, 0.f, 1.f);

	return vout;
}

uint GetSampleMask(in int index) {
	switch (index) {
	case 0: return gSampleMask0;
	case 1: return gSampleMask1;
	case 2: return gSampleMask2;
	case 3: return gSampleMask3;
	case 4: return gSampleMask4;
	}
	return 0;
}

Texture2D TextureObject(in int index) {
	switch (index) {
	case 0: return gi_Debug0;
	case 1: return gi_Debug1;
	case 2: return gi_Debug2;
	case 3: return gi_Debug3;
	case 4: return gi_Debug4;
	}
	return gi_Debug0;
}

Texture2D<uint> TextureObject_uint(in int index) {
	switch (index) {
	case 0: return gi_Debug0_uint;
	case 1: return gi_Debug1_uint;
	case 2: return gi_Debug2_uint;
	case 3: return gi_Debug3_uint;
	case 4: return gi_Debug4_uint;
	}
	return gi_Debug0_uint;
}

float4 SampleColor(in int index, in float2 tex) {
	const uint mask = GetSampleMask(index);
	switch (mask) {
	case Debug::SampleMask::RGB: {
		const Texture2D map = TextureObject(index);
		const float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).rgb;
		return float4(samp, 1.f);
	}
	case Debug::SampleMask::RG: {
		const Texture2D map = TextureObject(index);
		const float2 samp = map.SampleLevel(gsamPointClamp, tex, 0).rg;
		return float4(samp, 0.f, 1.f);
	}
	case Debug::SampleMask::RRR: {
		const Texture2D map = TextureObject(index);
		const float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).rrr;
		return float4(samp, 1.f);
	}
	case Debug::SampleMask::GGG: {
		const Texture2D map = TextureObject(index);
		const float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).ggg;
		return float4(samp, 1.f);
	}
	case Debug::SampleMask::AAA: {
		const Texture2D map = TextureObject(index);
		const float3 samp = map.SampleLevel(gsamPointClamp, tex, 0).aaa;
		return float4(samp, 1.f);
	}
	case Debug::SampleMask::FLOAT: {
		const Texture2D map = TextureObject(index);
		const float samp = map.SampleLevel(gsamPointClamp, tex, 0).x;
		const float finalColor = samp / cb_Debug.SampleDescs[index].Denominator;
		return finalColor >= 0.f ? lerp(cb_Debug.SampleDescs[index].MinColor, cb_Debug.SampleDescs[index].MaxColor, finalColor) : (float4)1.f;
	}
	case Debug::SampleMask::UINT: {
		const Texture2D<uint> map = TextureObject_uint(index);
		const uint samp = map.Load(int3(tex.x * 800, tex.y * 600, 0)).x;
		const float finalColor = min(1.f, samp / cb_Debug.SampleDescs[index].Denominator);
		return lerp(cb_Debug.SampleDescs[index].MinColor, cb_Debug.SampleDescs[index].MaxColor, finalColor);
	}
	}
	return (float4)1.f;
}

SDR_FORMAT PS(VertexOut pin) : SV_Target {
	switch (pin.InstID) {
	case 0: return SampleColor(0, pin.TexC);
	case 1: return SampleColor(1, pin.TexC);
	case 2: return SampleColor(2, pin.TexC);
	case 3: return SampleColor(3, pin.TexC);
	case 4: return SampleColor(4, pin.TexC);
	}
	return float4(1.f, 0, 1.f, 1.f);
}

#endif // __DEBUGTEXMAP_HLSL__