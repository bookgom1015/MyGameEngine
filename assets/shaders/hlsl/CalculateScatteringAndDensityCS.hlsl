#ifndef __CALCULATESCATTERINGANDDENSITY_HLSL__
#define __CALCULATESCATTERINGANDDENSITY_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

#include "VolumetricLight.hlsli"
#include "Shadow.hlsli"
#include "LightingUtil.hlsli"

ConstantBuffer<ConstantBuffer_Pass> cb_Pass : register(b0);

cbuffer cbRootConstants : register (b1) {
	float gNearZ;
	float gFarZ;
	float gDepthExponent;
	float gUniformDensity;
	float gAnisotropicCoefficient;
}

Texture2D<Shadow::ZDepthMapFormat>				gi_ZDepth[MaxLights]			: register(t0);
Texture2DArray<Shadow::ZDepthMapFormat>			gi_ZDepthCube[MaxLights]		: register(t0, space1);
Texture2DArray<Shadow::FaceIDCubeMapFormat>		gi_FaceIDTexArray[MaxLights]	: register(t0, space2);

RWTexture3D<VolumetricLight::FrustumMapFormat>	go_FrustumVolume				: register(u0);

float4x4 GetViewProjMatrix(Light light, uint lightIndex, float2 uv, uint texIndex) {
	const uint faceID = (uint)gi_FaceIDTexArray[lightIndex].SampleLevel(gsamPointClamp, float3(uv, texIndex), 0);
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

[numthreads(
	VolumetricLight::CalcScatteringAndDensity::ThreadGroup::Width, 
	VolumetricLight::CalcScatteringAndDensity::ThreadGroup::Height, 
	VolumetricLight::CalcScatteringAndDensity::ThreadGroup::Depth)]
void CS(uint3 DTid : SV_DispatchThreadId) {
	uint3 dims;
	go_FrustumVolume.GetDimensions(dims.x, dims.y, dims.z);
	if (all(DTid >= dims)) return;

	const float3 posW = ThreadIdToWorldPosition(DTid, dims, gDepthExponent, gNearZ, gFarZ, cb_Pass.InvView, cb_Pass.InvProj);
	const float3 toEyeW = normalize(cb_Pass.EyePosW - posW);

	float3 Li = 0.f; // Ambient lights;

	[loop]
	for (uint i = 0; i < cb_Pass.LightCount; ++i) {
		Light light = cb_Pass.Lights[i];

		float3 direction = 0.f;
		float Ld = 0.f;
		float falloff = 1.f;

		if (light.Type == LightType::E_Directional) {
			direction = light.Direction;
		}
		else if (light.Type == LightType::E_Tube || light.Type == LightType::E_Rect) {
			// Tube and rectangular light do not have shadow(depth) map, 
			//  so these can not calculate visibility.
			return;
		}
		else {
			direction = posW - light.Position;
			Ld = length(direction);
		}

		const bool needCube = light.Type == LightType::E_Point || light.Type == LightType::E_Spot;
		const float3 toLight = -direction;

		float visibility = 1.f;

		if (needCube) {
			const uint index = GetCubeFaceIndex(direction);
			const float3 normalized = normalize(direction);
			const float2 uv = ConvertDirectionToUV(normalized);

			const float4x4 viewProj = GetViewProjMatrix(light, i, uv, index);
			if (IsNotZeroMatrix(viewProj)) visibility = CalcShadowFactorCubeCS(gi_ZDepthCube[i], gsamShadow, viewProj, posW, uv, index);

			falloff = CalcAttenuation_InverseSquare(Ld, light.AttenuationRadius);
		}
		else if (light.Type == LightType::E_Directional) {
			visibility = CalcShadowFactorDirectional(gi_ZDepth[i], gsamShadow, light.Mat1, posW);
		}
		else {
			visibility = CalcShadowFactor(gi_ZDepth[i], gsamShadow, light.Mat0, posW);
		}

		const float phaseFunction = HenyeyGreensteinPhaseFunction(direction, toEyeW, gAnisotropicCoefficient);

		Li += visibility * light.Color * light.Intensity * falloff * phaseFunction;
	}

	go_FrustumVolume[DTid] = float4(Li * gUniformDensity, gUniformDensity);
}

#endif // __CALCULATESCATTERINGANDDENSITY_HLSL__