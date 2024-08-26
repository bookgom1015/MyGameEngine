#ifndef __DRAWSHADOWCS_HLSL__
#define __DRAWSHADOWCS_HLSL__

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

Texture2D<GBuffer::PositionMapFormat>			gi_Position			: register(t0);
Texture2D<Shadow::ZDepthMapFormat>				gi_ZDepth			: register(t1);
Texture2DArray<Shadow::ZDepthMapFormat>			gi_ZDepthTexArray	: register(t2);
Texture2DArray<Shadow::VSDepthCubeMapFormat>	gi_VSDepthTexArray	: register(t3);
Texture2DArray<Shadow::FaceIDCubeMapFormat>		gi_FaceIDTexArray	: register(t4);

RWTexture2D<Shadow::ShadowMapFormat>	guo_Shadow	: register(u0);
RWTexture2D<Shadow::DebugMapFormat>		guo_Debug	: register(u1);

float4x4 GetViewMatrix(Light light, float2 uv, uint index) {
	uint faceID = (uint)gi_FaceIDTexArray.SampleLevel(gsamPointClamp, float3(uv, index), 0);
	switch (faceID) {
	case 0: return light.View0;
	case 1: return light.View1;
	case 2: return light.View2;
	case 3: return light.View3;
	case 4: return light.View4;
	case 5: return light.View5;
	default: return (float4x4)0;
	}
}

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	uint value = gLightIndex != 0 ? guo_Shadow[DTid] : 0;

	Light light = cb_Pass.Lights[gLightIndex];

	const float4 posW = gi_Position.Load(int3(DTid, 0));

	if (light.Type == LightType::E_Point) {
		const float3 direction = posW.xyz - light.Position;
		const uint index = GetCubeFaceIndex(direction);
		const float3 normalized = normalize(direction);
		const float2 uv = ConvertDirectionToUV(normalized);

		const float4x4 view = GetViewMatrix(light, uv, index);
		const float4x4 viewProj = mul(view, light.Proj);
		const float shadowFactor = CalcShadowFactorTexArray(gi_ZDepthTexArray, gsamPointClamp, viewProj, posW.xyz, uv, index);

		value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
	}
	else {
		const float4 shadowPosH = mul(posW, light.Proj);
		const float shadowFactor = CalcShadowFactor(gi_ZDepth, gsamShadow, shadowPosH);

		value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
	}
	
	guo_Shadow[DTid] = value;
}

#endif // __DRAWSHADOWCS_HLSL__