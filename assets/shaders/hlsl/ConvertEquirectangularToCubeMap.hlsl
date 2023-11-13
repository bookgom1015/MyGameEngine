#ifndef __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__
#define __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "Samplers.hlsli"

ConstantBuffer<ConvertEquirectangularToCubeConstantBuffer>	cbCube	: register(b0);

cbuffer cbRootConstants : register(b1) {
	uint gFaceID;
}

Texture2D<float3> gi_Equirectangular : register(t0);

VERTEX_IN

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

static const float2 InvATan = float2(0.1591, 0.3183);

VertexOut VS(VertexIn vin) {
	VertexOut vout;

	vout.PosL = vin.PosL;

	float4x4 view = cbCube.View[gFaceID];
	float4 posV = mul(float4(vin.PosL, 1.0f), view);
	float4 posH = mul(posV, cbCube.Proj);

	vout.PosH = posH.xyww;

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
	float3 samp = gi_Equirectangular.SampleLevel(gsamLinearClamp, texc, 0);
	return float4(samp, 1);
}

#endif // __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__