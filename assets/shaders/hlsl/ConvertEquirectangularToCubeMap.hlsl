#ifndef __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__
#define __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants>	cbPass	: register(b0);
ConstantBuffer<ObjectConstants> cbObj	: register(b1);

Texture2D<float3> gi_Equirectangular : register(t0);

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
	float2 texc = SampleSphericalMap(normalize(pin.PosL));
	float3 samp = gi_Equirectangular.Sample(gsamLinearClamp, texc, 0);
	return float4(samp, 1);
}

#endif // __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__