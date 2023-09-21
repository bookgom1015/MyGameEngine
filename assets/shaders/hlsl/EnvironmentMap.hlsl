#ifndef __ENVIRONMENTMAP_HLSL__
#define __ENVIRONMENTMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants>	cbPass	: register(b0);
ConstantBuffer<ObjectConstants> cbObj	: register(b1);

TextureCube gi_Cube : register(t0);

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), cbObj.World);

	// Always center sky about camera.
	posW.xyz += cbPass.EyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(posW, cbPass.ViewProj).xyww;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	float4 cubeSample = gi_Cube.Sample(gsamLinearWrap, normalize(pin.PosL));

	return cubeSample;
}

#endif // __ENVIRONMENTMAP_HLSL__