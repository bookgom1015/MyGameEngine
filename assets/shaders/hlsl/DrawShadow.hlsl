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

Texture2D<GBuffer::PositionMapFormat>		gi_Position			: register(t0);
Texture2D<Shadow::ZDepthMapFormat>			gi_ZDepth			: register(t1);
Texture2DArray<Shadow::ZDepthMapFormat>		gi_ZDepthTexArray	: register(t2);
Texture2DArray<Shadow::FaceIDCubeMapFormat>	gi_FaceIDTexArray	: register(t3);

RWTexture2D<Shadow::ShadowMapFormat>	guo_Shadow	: register(u0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

float4x4 GetShadowTransform(Light light, float2 uv, uint index) {
	float faceID = gi_FaceIDTexArray.SampleLevel(gsamPointClamp, float3(uv, index), 0);
	uint u_faceID = (uint)faceID;
	switch (u_faceID) {
	case 0: return light.ShadowTransform0;
	case 1: return light.ShadowTransform1;
	case 2: return light.ShadowTransform2;
	case 3: return light.ShadowTransform3;
	case 4: return light.ShadowTransform4;
	case 5: return light.ShadowTransform5;
	default: return (float4x4)0;
	}
}

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

void PS(VertexOut pin) {
	uint2 size;
	gi_Position.GetDimensions(size.x, size.y);

	uint2 DTid = (pin.TexC - 0.5) * size;

	uint value = 0;
	if (gLightIndex != 0) value = guo_Shadow[DTid];

	Light light = cb_Pass.Lights[gLightIndex];

	const float4 posW = gi_Position.Load(int3(DTid, 0));

	if (light.Type == LightType::E_Point) {
		const float3 direction = posW.xyz - light.Position;
		const uint index = GetCubeFaceIndex(direction);
		const float3 normalized = normalize(direction);
		const float2 uv = ConvertDirectionToUV(normalized);

		const float4x4 shadowTransform = GetShadowTransform(light, uv, index);
		const float shadowFactor = CalcShadowFactorTexArray(gi_ZDepthTexArray, gsamPointClamp, shadowTransform, posW.xyz, uv, index);

		value = (uint)shadowFactor;
	}
	else {
		const float4 shadowPosH = mul(posW, light.ShadowTransform0);
		const float shadowFactor = CalcShadowFactor(gi_ZDepth, gsamShadow, shadowPosH);

		value = CalcShiftedShadowValueF(shadowFactor, value, gLightIndex);
	}

	guo_Shadow[DTid] = value;
}

#endif // __DRAWSHADOW_HLSL__