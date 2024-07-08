#ifndef __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__
#define __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Irradiance> cb_Irrad	: register(b0);

cbuffer cbRootConstants : register(b1) {
	uint gFaceID;
}

Texture2D<HDR_FORMAT> gi_Equirectangular : register(t0);

#include "HardCodedCubeVertices.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

static const float2 InvATan = float2(0.1591, 0.3183);

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	float3 posL = gVertices[vid];

	vout.PosL = posL;

	float4x4 view = cb_Irrad.View[gFaceID];
	float4 posV = mul(float4(posL, 1), view);
	float4 posH = mul(posV, cb_Irrad.Proj);

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

HDR_FORMAT PS(VertexOut pin) : SV_Target{
	float2 texc = SampleSphericalMap(normalize(pin.PosL));
	float3 samp = gi_Equirectangular.Sample(gsamLinearClamp, texc).rgb;
	return float4(samp, 1);
}

#endif // __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__