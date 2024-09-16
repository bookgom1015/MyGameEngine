//***************************************************************************************
// LightingUtil.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Contains API for shader lighting.
//***************************************************************************************
#ifndef __LIGHTINGUTIL_HLSLI__
#define __LIGHTINGUTIL_HLSLI__

#include "./../../../include/HlslCompaction.h"
#include "BRDF.hlsli"

static const float DEG2RAD = 3.14159265359 / 180.0;

float CalcAttenuation(float d, float attenStart, float attenEnd) {
	return saturate((attenEnd - d) / (attenEnd - attenStart));
}

float DegToRad(float degrees) {
	return degrees * DEG2RAD;
}

float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye) {
	const float3 lightDir = normalize(-L.Direction);
	const float3 lightStrength = L.LightColor * L.Intensity;

#if defined(BLINN_PHONG)
	return BlinnPhong(lightStrength, lightDir, normal, toEye, mat);
#elif defined(COOK_TORRANCE)
	return CookTorrance(lightStrength, lightDir, normal, toEye, mat);
#else
	return 0;
#endif
}

float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
	const float3 lightVec = L.Position - pos;
	const float distance = length(lightVec);

	const float3 lightDir = lightVec / distance;

	const float X = pow(distance / L.AttenuationRadius, 4);
	const float numer = pow(saturate(1 - X), 2);
	const float denom = distance * distance + 1;

	const float falloff = numer / denom;
	float3 lightStrength = L.LightColor * L.Intensity * falloff;

#if defined(BLINN_PHONG)
	return BlinnPhong(lightStrength, lightDir, normal, toEye, mat);
#elif defined(COOK_TORRANCE)
	return CookTorrance(lightStrength, lightDir, normal, toEye, mat);
#else
	return 0;
#endif
}

float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye) {
	const float3 lightVec = L.Position - pos;
	const float3 lightDir = normalize(lightVec);

	const float theta = dot(-lightDir, L.Direction);

	const float radOuter = DegToRad(L.OuterConeAngle);
	const float cosOuter = cos(radOuter);

	if (theta < cosOuter) return 0;

	const float radInner = DegToRad(L.InnerConeAngle);
	const float cosInner = cos(radInner);

	const float epsilon = cosInner - cosOuter;
	const float factor = clamp((theta - cosOuter) / epsilon, 0, 1);

	const float distance = length(lightVec);

	const float X = pow(distance / L.AttenuationRadius, 4);
	const float numer = pow(saturate(1 - X), 2);
	const float denom = distance * distance + 1;

	const float falloff = numer / denom;
	const float3 lightStrength = L.LightColor * L.Intensity * factor * falloff;

#if defined(BLINN_PHONG)
	return BlinnPhong(lightStrength, lightDir, normal, toEye, mat);
#elif defined(COOK_TORRANCE)
	return CookTorrance(lightStrength, lightDir, normal, toEye, mat);
#else
	return 0;
#endif
}

float3 ComputeBRDF(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye,	float shadowFactor[MaxLights], uint lightCount) {
	float3 result = 0;

	[loop]
	for (uint i = 0; i < lightCount; ++i) {
		Light light = gLights[i];
		const float factor = shadowFactor[i];
		if (light.Type == LightType::E_Directional) result += factor * ComputeDirectionalLight(light, mat, normal, toEye);
		else if (light.Type == LightType::E_Point) result += factor * ComputePointLight(light, mat, pos, normal, toEye);
		else if (light.Type == LightType::E_Spot) result += factor * ComputeSpotLight(light, mat, pos, normal, toEye);
	}

	return result;
}

#endif // __LIGHTINGUTIL_HLSLI__