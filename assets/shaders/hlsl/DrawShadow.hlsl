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

Texture2D<GBuffer::PositionMapFormat>	gi_Position		: register(t0);
Texture2D<Shadow::ZDepthMapFormat>		gi_ZDepth		: register(t1);
TextureCube<Shadow::ZDepthMapFormat>	gi_ZDepthCube	: register(t2);

RWTexture2D<Shadow::ShadowMapFormat>	guo_Shadow	: register(u0);

float4x4 GetShadowTransform(Light light, int face) {
	switch (face) {
	case 0: return light.ShadowTransform0;
	case 1: return light.ShadowTransform1;
	case 2: return light.ShadowTransform2;
	case 3: return light.ShadowTransform3;
	case 4: return light.ShadowTransform4;
	case 5: return light.ShadowTransform5;
	default: return (float4x4)0;
	}
}

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	uint value = 0;
	if (gLightIndex != 0) value = guo_Shadow[DTid];

	Light light = cb_Pass.Lights[gLightIndex];

	const float4 posW = gi_Position.Load(int3(DTid, 0));
	
	if (light.Type == LightType::E_Point) {
		for (int face = 0; face < 1; ++face) {
			const float4x4 shadowTransform = GetShadowTransform(light, face);
			const float4 shadowPosH = mul(posW, shadowTransform);
			const float shadowFactor = CalcShadowFactorCube(gi_ZDepthCube, gsamLinearWrap, shadowTransform, posW.xyz, light.Position);

			value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
		}
	}
	else {
		const float4 shadowPosH = mul(posW, light.ShadowTransform0);
		const float shadowFactor = CalcShadowFactor(gi_ZDepth, gsamShadow, shadowPosH);

		value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
	}

	guo_Shadow[DTid] = value;
}

#endif // __DRAWSHADOW_HLSL__