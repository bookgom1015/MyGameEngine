#ifndef __DRAWSHADOW_HLSL__
#define __DRAWSHADOW_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Pass>		cb_Pass	: register(b0);

cbuffer cbRootConstants : register(b1) {
	uint gLightIndex;
};

Texture2D<GBuffer::PositionMapFormat>	gi_Position	: register(t0);
Texture2D<Shadow::ZDepthMapFormat>		gi_ZDepth	: register(t1);

RWTexture2D<Shadow::ShadowMapFormat>	guo_Shadow	: register(u0);

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	uint value = 0;
	if (gLightIndex != 0) value = guo_Shadow[DTid];

	float4 posW = gi_Position.Load(int3(DTid, 0));
	const float4 shadowPosH = mul(posW, cb_Pass.Lights[gLightIndex].ShadowTransform);
	float shadowFactor = CalcShadowFactor(gi_ZDepth, gsamShadow, shadowPosH);
	
	uint shifted = (uint)shadowFactor << gLightIndex;
	value = value | shifted;

	guo_Shadow[DTid] = value;
}

#endif // __DRAWSHADOW_HLSL__