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

EquirectangularConverter_ConvCubeToEquirect_RootConstants(b0)

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target{
	const float3 sampVec = Equirectangular::SphericalToCartesian(pin.TexC);
	const float3 samp = gi_Cube.SampleLevel(gsamLinearClamp, sampVec, gMipLevel);
	return float4(samp, 1.f);
}

#endif // __CONVERTECUBEMAPEQUIRECTANGULAR_HLSL__