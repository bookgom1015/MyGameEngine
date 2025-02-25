#ifndef __DEBUGCUBEMAP_HLSL__
#define __DEBUGCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

#include "ValueTypeConversion.hlsli"
#include "Equirectangular.hlsli"

ConstantBuffer<ConstantBuffer_Pass>				cb_Pass				: register(b0);

Debug_DebugCubeMap_RootConstants(b1)

TextureCube<IrradianceMap::EnvCubeMapFormat>	gi_Cube				: register(t0);
Texture2D<IrradianceMap::EquirectMapFormat>		gi_Equirectangular	: register(t1);

#include "HardCodedCubeVertices.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

static const float2 InvATan = float2(0.1591f, 0.3183f);

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	const float3 posL = gVertices[vid];

	vout.PosL = posL;
	vout.PosH = mul(float4(posL, 1.f), cb_Pass.ViewProj);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target{
	float4 color = (float4)0.f;

#ifdef SPHERICAL
	const float2 texc = Equirectangular::SampleSphericalMap(normalize(pin.PosL));
	color = gi_Equirectangular.SampleLevel(gsamLinearClamp, texc, gMipLevel);
#else
	color = gi_Cube.SampleLevel(gsamLinearWrap, normalize(pin.PosL), gMipLevel);
#endif

	return LinearTosRGB(color);
}

#endif // __DEBUGCUBEMAP_HLSL__