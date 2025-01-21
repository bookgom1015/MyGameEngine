#ifndef __DEBUGCUBEMAP_HLSL__
#define __DEBUGCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "ValueTypeConversion.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Pass>		cb_Pass	: register(b0);

cbuffer cbRootConstants : register(b2) {
	float gMipLevel;
}

TextureCube<IrradianceMap::EnvCubeMapFormat>	gi_Cube				: register(t0);
Texture2D<IrradianceMap::EquirectMapFormat>		gi_Equirectangular	: register(t1);

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
	vout.PosH = mul(float4(posL, 1), cb_Pass.ViewProj);

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
	float4 color = 0;

#ifdef SPHERICAL
	float2 texc = SampleSphericalMap(normalize(pin.PosL));
	color = gi_Equirectangular.SampleLevel(gsamLinearClamp, texc, gMipLevel);
#else
	color = gi_Cube.SampleLevel(gsamLinearWrap, normalize(pin.PosL), gMipLevel);
#endif

	float4 gammaEncoded = LinearTosRGB(color);

	return gammaEncoded;
}

#endif // __DEBUGCUBEMAP_HLSL__