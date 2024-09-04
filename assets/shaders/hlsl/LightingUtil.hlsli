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
	float3 lightVec = normalize(-L.Direction);

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0);
	float3 lightStrength = L.LightColor * L.Intensity;

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
		return 0;

	// Normalize the light vector.
	lightVec /= d;

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0);
	float3 lightStrength = L.LightColor * L.Intensity;

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
		return 0;

	// Normalize the light vector.
	lightVec /= d;

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0);
	float3 lightStrength = L.LightColor * L.Intensity;

	// Attenuate light by distance.
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	// Scale by spotlight
	float spotFactor = pow(max(dot(-lightVec, L.Direction), 0), L.SpotPower);
	lightStrength *= spotFactor;

#if defined(BLINN_PHONG)
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
#elif defined(COOK_TORRANCE)
	return CookTorrance(lightStrength, lightVec, normal, toEye, mat);
#else
	return 0;
#endif
}

float3 ComputeBRDF(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye,	float shadowFactor[MaxLights], uint lightCount) {
	float3 result = 0;

	[loop]
	for (uint i = 0; i < lightCount; ++i) {
		Light light = gLights[i];
		if (light.Type == LightType::E_Directional) {
			const float factor = shadowFactor[i];
			result += factor * ComputeDirectionalLight(light, mat, normal, toEye);
		}
		else if (light.Type == LightType::E_Point) {
			const float factor = shadowFactor[i];
			result += factor * ComputePointLight(light, mat, pos, normal, toEye);
		}
		else if (light.Type == LightType::E_Spot) {
			const float factor = shadowFactor[i];
			result += factor * ComputeSpotLight(light, mat, pos, normal, toEye);
		}
	}

	return result;
}

#endif // __LIGHTINGUTIL_HLSLI__