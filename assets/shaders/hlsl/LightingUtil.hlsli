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
	float3 lightStrength = L.Strength * ndotl;

#ifdef BLINN_PHONG
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
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
	float3 lightStrength = L.Strength * ndotl;

	// Attenuate light by distance.
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

#ifdef BLINN_PHONG
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
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
	float3 lightStrength = L.Strength * ndotl;

	// Attenuate light by distance.
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	// Scale by spotlight
	float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
	lightStrength *= spotFactor;

#ifdef BLINN_PHONG
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
#else
	return 0;
#endif
}

float3 ComputeBRDF(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye,	float3 shadowFactor) {
	float3 result = 0;

	int i = 0;

#if (NUM_DIR_LIGHTS > 0)
	for (i = 0; i < NUM_DIR_LIGHTS; ++i) {
		float factor = shadowFactor[i];
		result += factor * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
	}
#endif

#if (NUM_POINT_LIGHTS > 0)
	for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i) {
		result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
	}
#endif

#if (NUM_SPOT_LIGHTS > 0)
	for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i) {
		result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
	}
#endif 

	return result;
}

#endif // __LIGHTINGUTIL_HLSLI__