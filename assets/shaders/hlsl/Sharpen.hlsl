#ifndef __SHARPEN_HLSL__
#define __SHARPEN_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

cbuffer cbRootConstants : register(b0) {
	float2	gInvTexSize;
	float	gAmount;
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
	float neighbor = gAmount * -1;
	float center = gAmount * 4 + 1;

	float dx = gInvTexSize.x;
	float dy = gInvTexSize.y;

	float3 curr		= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC, 0).rgb * center;
	float3 up		= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2(0,  dy), 0).rgb * neighbor;
	float3 down		= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2(0, -dy), 0).rgb * neighbor;
	float3 left		= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2(-dx, 0), 0).rgb * neighbor;
	float3 right	= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2( dx, 0), 0).rgb * neighbor;

	float3 finalColor = curr + up + down + left + right;

	return float4(finalColor, 1);
}

#endif // __SHARPEN_HLSL__