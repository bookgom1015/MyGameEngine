// [ References ]
//  - https://learnopengl.com/PBR/IBL/Diffuse-irradiance
//  - https://learnopengl.com/PBR/IBL/Specular-IBL

#ifndef __CONVERTECUBEMAPEQUIRECTANGULAR_HLSL__
#define __CONVERTECUBEMAPEQUIRECTANGULAR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

#include "Equirectangular.hlsli"

TextureCube<float3> gi_Cube : register(t0);

cbuffer cbRootConstants : register(b0) {
	uint gMipLevel;
}

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	float3 sampVec = Equirectangular::SphericalToCartesian(pin.TexC);
	float3 samp = gi_Cube.SampleLevel(gsamLinearClamp, sampVec, gMipLevel);

	return float4(samp, 1);
}

#endif // __CONVERTECUBEMAPEQUIRECTANGULAR_HLSL__