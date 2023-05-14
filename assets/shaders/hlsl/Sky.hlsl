#ifndef __SKY_HLSL__
#define __SKY_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants> cb : register(b0);

Texture2D gi_Cube : register(t0);

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), cb.World);

	// Always center sky about camera.
	posW.xyz += cb.EyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(posW, cb.ViewProj).xyww;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	return gi_Cube.Sample(gsamLinearWrap, pin.PosL);
}

#endif // __SKY_HLSL__