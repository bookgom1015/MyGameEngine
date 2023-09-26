#ifndef __DRAWCUBEMAP_HLSL__
#define __DRAWCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants>	cbPass	: register(b0);
ConstantBuffer<ObjectConstants> cbObj	: register(b1);

cbuffer cbRootConstants : register(b2) {
	float gMipLevel;
}

TextureCube<float3> gi_Cube				: register(t0);
Texture2D<float3>	gi_Equirectangular	: register(t1);

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

static const float2 InvATan = float2(0.1591, 0.3183);

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout;

	vout.PosL = vin.PosL;

	float4 posW = mul(float4(vin.PosL, 1.0f), cbObj.World);

	vout.PosH = mul(posW, cbPass.ViewProj);

	return vout;
}

float2 SampleSphericalMap(float3 view) {
	float2 texc = float2(atan2(view.z, view.x), asin(view.y));
	texc *= InvATan;
	texc += 0.5;
	texc.y = 1 - texc.y;
	return texc;
}

float4 PS(VertexOut pin) : SV_Target{
	float3 samp = 0;

#ifdef SPHERICAL
	float2 texc = SampleSphericalMap(normalize(pin.PosL));
	 samp = gi_Equirectangular.SampleLevel(gsamLinearClamp, texc, 0);
#else
	samp = gi_Cube.SampleLevel(gsamLinearWrap, normalize(pin.PosL), gMipLevel);
#endif

	return float4(samp, 1);
}

#endif // __DRAWCUBEMAP_HLSL__