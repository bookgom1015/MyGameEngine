#ifndef __CONVERTECUBEMAPEQUIRECTANGULAR_HLSL__
#define __CONVERTECUBEMAPEQUIRECTANGULAR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

TextureCube<float3> gi_Cube : register(t0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

static const float2 InvATan = float2(0.1591, 0.3183);

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

float3 SphericalToCartesian(float2 sphericalCoord) {
	// ±¸¸é ÁÂÇ¥¿¡¼­ ÅØ½ºÃ³ ÁÂÇ¥·Î º¯È¯
	sphericalCoord.y = 1 - sphericalCoord.y;
	sphericalCoord -= 0.5;
	sphericalCoord /= InvATan;

	// ÅØ½ºÃ³ ÁÂÇ¥¸¦ 3D °ø°£ ÁÂÇ¥·Î º¯È¯
	float3 cartesianCoord;
	cartesianCoord.x = cos(sphericalCoord.x) * cos(sphericalCoord.y);
	cartesianCoord.y = sin(sphericalCoord.y);
	cartesianCoord.z = sin(sphericalCoord.x) * cos(sphericalCoord.y);

	return cartesianCoord;
}

float4 PS(VertexOut pin) : SV_Target{
	float3 sampVec = SphericalToCartesian(pin.TexC);
	float3 samp = gi_Cube.SampleLevel(gsamLinearClamp, sampVec, 0);

	return float4(samp, 1);
}

#endif // __CONVERTECUBEMAPEQUIRECTANGULAR_HLSL__