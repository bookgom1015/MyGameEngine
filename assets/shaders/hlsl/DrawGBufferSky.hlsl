#ifndef __DRAWGBUFFERSKY_HLSL__
#define __DRAWGBUFFERSKY_HLSL__

#include "Common.hlsli"

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH			: SV_POSITION;
	float4 CurrPosH		: POSITION0;
	float4 PrevPosH		: POSITION1;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0.0f;

	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld) + float4(gEyePosW, 0.0f);

	vout.PosH = mul(posW, gViewProj).xyww;
	vout.CurrPosH = mul(posW, gViewProj).xyww;
	vout.PrevPosH = mul(posW, gPrevViewProj).xyww;

	return vout;
}

float4 PS(VertexOut pin) : SV_TARGET {
	pin.CurrPosH /= pin.CurrPosH.w;
	pin.PrevPosH /= pin.PrevPosH.w;
	
	float2 velocity = CalcVelocity(pin.CurrPosH, pin.PrevPosH);

	return float4(velocity, 0.0f, 1.0f);
}

#endif // __DRAWGBUFFERSKY_HLSL__