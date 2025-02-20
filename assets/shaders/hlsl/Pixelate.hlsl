#ifndef __PIXELATION_HLSL__
#define __PIXELATION_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

Pixelation_Default_RootConstants(b0)

Texture2D<SDR_FORMAT> gi_BackBuffer : register(t0);

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

float4 PS(VertexOut pin) : SV_Target{
	const uint2 indices = pin.TexC * gTexSize - 0.5f;
	
	const uint pixelSize = (uint)gPixelSize;

	float x = int(indices.x) % pixelSize;
	float y = int(indices.y) % pixelSize;

	x = floor(pixelSize / 2.f) - x;
	y = floor(pixelSize / 2.f) - y;

	x = indices.x + x;
	y = indices.y + y;

	const float2 texc = float2(x, y) / gTexSize;
	
	return gi_BackBuffer.SampleLevel(gsamLinearClamp, texc, 0);
}

#endif // __PIXELATION_HLSL__