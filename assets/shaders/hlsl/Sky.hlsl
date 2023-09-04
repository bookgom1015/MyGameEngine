#ifndef __SKY_HLSL__
#define __SKY_HLSL__

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

struct PixelOut {
	float4 BackBuffer	: SV_TARGET0;
	float4 Diffuse		: SV_TARGET1;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
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

PixelOut PS(VertexOut pin){
	float4 color = gi_Cube.Sample(gsamLinearWrap, pin.PosL);

	PixelOut pout = (PixelOut)0;
	pout.BackBuffer = color;
	pout.Diffuse = color;

	return pout;
}

#endif // __SKY_HLSL__