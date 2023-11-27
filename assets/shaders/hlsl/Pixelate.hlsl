#ifndef __PIXELATION_HLSL__
#define __PIXELATION_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

cbuffer cbRootConstants : register(b0) {
	float2 gTexSize;
	float gPixelSize;
}

Texture2D<SDR_FORMAT> gi_BackBuffer : register(t0);

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	float2 pos = float2(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y);
	vout.PosH = float4(pos, 0, 1);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	const uint2 indices = pin.TexC * gTexSize - 0.5;
	
	const uint pixelSize = (uint)gPixelSize;

	float x = int(indices.x) % pixelSize;
	float y = int(indices.y) % pixelSize;

	x = floor(pixelSize / 2.0) - x;
	y = floor(pixelSize / 2.0) - y;

	x = indices.x + x;
	y = indices.y + y;

	float2 texc = float2(x, y) / gTexSize;
	
	return gi_BackBuffer.SampleLevel(gsamLinearClamp, texc, 0);
}

#endif // __PIXELATION_HLSL__