#ifndef __SHARPEN_HLSL__
#define __SHARPEN_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "CoordinatesFittedToScreen.hlsli"

Sharpen_Default_RootConstants(b0)

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
	const float neighbor = gAmount * -1.f;
	const float center = gAmount * 4.f + 1.f;

	const float dx = gInvTexSize.x;
	const float dy = gInvTexSize.y;

	const float3 curr	= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC,					  0.f).rgb * center;
	const float3 up		= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2(0.f,  dy), 0.f).rgb * neighbor;
	const float3 down	= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2(0.f, -dy), 0.f).rgb * neighbor;
	const float3 left	= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2(-dx, 0.f), 0.f).rgb * neighbor;
	const float3 right	= gi_BackBuffer.SampleLevel(gsamLinearClamp, pin.TexC + float2( dx, 0.f), 0.f).rgb * neighbor;

	const float3 finalColor = curr + up + down + left + right;

	return float4(finalColor, 1.f);
}

#endif // __SHARPEN_HLSL__