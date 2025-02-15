#ifndef __SHADOWCS_HLSL__
#define __SHADOWCS_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

#include "Shadow.hlsli"

ConstantBuffer<ConstantBuffer_Pass>		cb_Pass	: register(b0);

cbuffer cbRootConstants : register(b1) {
	uint gLightIndex;
};

Texture2D<GBuffer::PositionMapFormat>			gi_Position			: register(t0);
Texture2D<Shadow::ZDepthMapFormat>				gi_ZDepth			: register(t1);
Texture2DArray<Shadow::ZDepthMapFormat>			gi_ZDepthTexArray	: register(t2);
Texture2DArray<Shadow::FaceIDCubeMapFormat>		gi_FaceIDTexArray	: register(t3);

RWTexture2D<Shadow::ShadowMapFormat>	guo_Shadow	: register(u0);
RWTexture2D<Shadow::DebugMapFormat>		guo_Debug	: register(u1);

float4x4 GetViewProjMatrix(Light light, float2 uv, uint index) {
	const uint faceID = (uint)gi_FaceIDTexArray.SampleLevel(gsamPointClamp, float3(uv, index), 0);
	switch (faceID) {
	case 0: return light.Mat0;
	case 1: return light.Mat1;
	case 2: return light.Mat2;
	case 3: return light.Mat3;
	case 4: return light.Mat4;
	case 5: return light.Mat5;
	default: return (float4x4)0;
	}
}

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	uint value = gLightIndex != 0 ? guo_Shadow[DTid] : 0;

	Light light = cb_Pass.Lights[gLightIndex];

	const float4 posW = gi_Position.Load(uint3(DTid, 0));

	const bool needCube = light.Type == LightType::E_Point || light.Type == LightType::E_Spot;

	if (needCube) {
		const float3 direction = posW.xyz - light.Position;
		const uint index = GetCubeFaceIndex(direction);
		const float3 normalized = normalize(direction);
		const float2 uv = ConvertDirectionToUV(normalized);

		const float4x4 viewProj = GetViewProjMatrix(light, uv, index);
		const float shadowFactor = all(viewProj != (float4x4)0) ? CalcShadowFactorCubeCS(gi_ZDepthTexArray, gsamShadow, viewProj, posW.xyz, uv, index) : 1.f;

		value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
	}
	else if (light.Type == LightType::E_Directional) {
		const float shadowFactor = CalcShadowFactorDirectional(gi_ZDepth, gsamShadow, light.Mat1, posW.xyz);

		value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
	}
	else {
		const float shadowFactor = CalcShadowFactorDirectional(gi_ZDepth, gsamShadow, light.Mat0, posW.xyz);

		value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
	}
	
	guo_Shadow[DTid] = value;
}

#endif // __SHADOWCS_HLSL__