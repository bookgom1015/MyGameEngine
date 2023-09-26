#ifndef __COPYSPECULARIRRADIANCE_HLSL__
#define __COPYSPECULARIRRADIANCE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConvertEquirectangularToCubeConstantBuffer> cbCube : register(b0);

cbuffer cbRootConstants : register(b1) {
	uint gFaceID;
	uint gMipLevel;
}

TextureCube<float3> gi_Cube : register(t0);

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

	vout.PosL = vin.PosL;

	float4x4 view = cbCube.View[gFaceID];
	float4 posV = mul(float4(vin.PosL, 1.0f), view);
	float4 posH = mul(posV, cbCube.Proj);

	vout.PosH = posH.xyww;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	float3 irradiance = gi_Cube.SampleLevel(gsamLinearClamp, normalize(pin.PosL), (float)gMipLevel);

	return float4(irradiance, 1);
}

#endif // __COPYSPECULARIRRADIANCE_HLSL__