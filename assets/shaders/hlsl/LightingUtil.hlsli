//***************************************************************************************
// LightingUtil.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Contains API for shader lighting.
//***************************************************************************************
#ifndef __LIGHTINGUTIL_HLSLI__
#define __LIGHTINGUTIL_HLSLI__

#include "./../../../include/HlslCompaction.h"
#include "BRDF.hlsli"

float CalcAttenuation(float d, float falloffStart, float falloffEnd) {
	// Linear falloff.
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye) {
	// The light vector aims opposite the direction the light rays travel.
	float3 lightVec = -L.Direction;

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength;

#if defined(BLINN_PHONG)
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
#elif defined(COOK_TORRANCE)
	return CookTorrance(lightStrength, lightVec, normal, toEye, mat);
#else
	return 0;
#endif
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
//---------------------------------------------------------------------------------------
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
	// The vector from the surface to the light.
	float3 lightVec = L.Position - pos;

	// The distance from surface to light.
	float d = length(lightVec);

	// Range test.
	if (d > L.FalloffEnd)
		return 0.0f;

	// Normalize the light vector.
	lightVec /= d;

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength;

	// Attenuate light by distance.
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

#if defined(BLINN_PHONG)
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
#elif defined(COOK_TORRANCE)
	return CookTorrance(lightStrength, lightVec, normal, toEye, mat);
#else
	return 0;
#endif
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for spot lights.
//---------------------------------------------------------------------------------------
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
	// The vector from the surface to the light.
	float3 lightVec = L.Position - pos;

	// The distance from surface to light.
	float d = length(lightVec);

	// Range test.
	if (d > L.FalloffEnd)
		return 0.0f;

	// Normalize the light vector.
	lightVec /= d;

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength;

	// Attenuate light by distance.
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	// Scale by spotlight
	float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
	lightStrength *= spotFactor;

#if defined(BLINN_PHONG)
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
#elif defined(COOK_TORRANCE)
	return CookTorrance(lightStrength, lightVec, normal, toEye, mat);
#else
	return 0;
#endif
}

float3 ComputeBRDF(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye,	float3 shadowFactor,
		uint numDirLights, uint numPointLights, uint numSpotLights) {
	float3 result = 0;

	uint i = 0;
	uint cnt = 0;

	cnt += numDirLights;
	[loop]
	for (; i < cnt; ++i) {
		float factor = shadowFactor[i];
		result += factor * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
	}

	cnt += numPointLights;
	[loop]
	for (; i < cnt; ++i) {
		result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
	}

	cnt += numSpotLights;
	[loop]
	for (; i < cnt; ++i) {
		result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
	}

	return result;
}

#endif // __LIGHTINGUTIL_HLSLI__