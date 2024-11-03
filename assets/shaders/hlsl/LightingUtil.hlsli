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

float DegToRad(float degrees) {
	return degrees * DEG2RAD;
}

float CalcLinearAttenuation(float d, float attenStart, float attenEnd) {
	return saturate((attenEnd - d) / (attenEnd - attenStart));
}

float3 CalcPlaneIntersection(float3 pos, float3 r, float3 dir, float3 center) {
	return pos + r * (dot(dir, center - pos) / dot(dir, r));
}

float3 ComputeDirectionalLight(Light light, Material mat, float3 normal, float3 toEye) {
	const float3 lightVec = -light.Direction;
	const float3 Li = light.Color * light.Intensity;

#if defined(BLINN_PHONG)
	return BlinnPhong(mat, Li, lightVec, normal, toEye);
#elif defined(COOK_TORRANCE)
	return CookTorrance(mat, Li, lightVec, normal, toEye);
#else
	return 0;
#endif
}

float3 ComputePointLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye) {
	const float3 lightVec = light.Position - pos;
	const float distance = length(lightVec);

	const float X = pow(distance / light.AttenuationRadius, 4);
	const float numer = pow(saturate(1 - X), 2);
	const float denom = distance * distance + 1;

	const float falloff = numer / denom;
	const float3 Li = light.Color * light.Intensity * falloff;

	const float3 r = reflect(-toEye, normal);
	const float3 centerToRay = dot(lightVec, r) * r - lightVec;
	const float3 closestPoint = lightVec + centerToRay * saturate(light.Radius / length(centerToRay));
	const float3 L = normalize(closestPoint);

#if defined(BLINN_PHONG)
	return BlinnPhong(mat, Li, lightVec, normal, toEye);
#elif defined(COOK_TORRANCE)
	return CookTorrance_GGXModified(mat, Li, L, normal, toEye, distance, light.Radius);
#else
	return 0;
#endif
}

float3 ComputeSpotLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye) {
	const float3 lightVec = light.Position - pos;
	const float3 lightDir = normalize(lightVec);

	const float theta = dot(-lightDir, light.Direction);

	const float radOuter = DegToRad(light.OuterConeAngle);
	const float cosOuter = cos(radOuter);

	if (theta < cosOuter) return 0;

	const float radInner = DegToRad(light.InnerConeAngle);
	const float cosInner = cos(radInner);

	const float epsilon = cosInner - cosOuter;
	const float factor = clamp((theta - cosOuter) / epsilon, 0, 1);

	const float distance = length(lightVec);

	const float X = pow(distance / light.AttenuationRadius, 4);
	const float numer = pow(saturate(1 - X), 2);
	const float denom = distance * distance + 1;

	const float falloff = numer / denom;
	const float3 Li = light.Color * light.Intensity * factor * falloff;

#if defined(BLINN_PHONG)
	return BlinnPhong(mat, Li, lightVec, normal, toEye);
#elif defined(COOK_TORRANCE)
	return CookTorrance(mat, Li, lightVec, normal, toEye);
#else
	return 0;
#endif
}

float3 ComputeTubeLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye) {
	const float3 L0 = light.Position - pos;
	const float3 L1 = light.Position1 - pos;

	const float d0 = length(L0);
	const float d1 = length(L1);

	const float NdotL0 = dot(normal, L0) / (2 * d0);
	const float NdotL1 = dot(normal, L1) / (2 * d1);
	float NdotL = (2 * saturate(NdotL0 + NdotL1)) / (d0 * d1 + dot(L0, L1) + 2);

	const float3 r = reflect(-toEye, normal);
	const float3 Ldiff = L1 - L0;
	const float RdotLdiff = dot(r, Ldiff);
	const float Ld = length(Ldiff);
	const float t = (dot(r, L0) * RdotLdiff - dot(L0, Ldiff)) / (Ld * Ld - RdotLdiff * RdotLdiff);

	float3 closestPoint = L0 + Ldiff * saturate(t);
	const float3 centerToRay = dot(closestPoint, r) * r - closestPoint;
	closestPoint = closestPoint + centerToRay * saturate(light.Radius / length(centerToRay));

	const float3 L = normalize(closestPoint);
	const float distance = length(closestPoint);

	const float X = pow(distance / light.AttenuationRadius, 4);
	const float numer = pow(saturate(1 - X), 2);
	const float denom = distance * distance + 1;
	
	const float falloff = numer / denom;
	const float3 Li = light.Color * light.Intensity * falloff;

#if defined(BLINN_PHONG)
	return 0;
#elif defined(COOK_TORRANCE)
	return CookTorrance_GGXModified(mat, Li, L, normal, toEye, distance, light.Radius);
#else
	return 0;
#endif
}

float3 ComputeRectLight(Light light, Material mat, float3 pos, float3 normal, float3 toEye) {
	const float3 L0 = light.Position - pos;
	const float3 L1 = light.Position1 - pos;
	const float3 L2 = light.Position2 - pos;
	const float3 L3 = light.Position3 - pos;
	
	const float facingCheck = dot(L0, cross(light.Position2 - light.Position, light.Position1 - light.Position));
	if (facingCheck > 0) return 0;

	const float3 n0 = normalize(cross(L0, L1));
	const float3 n1 = normalize(cross(L1, L2));
	const float3 n2 = normalize(cross(L2, L3));
	const float3 n3 = normalize(cross(L3, L0));

	const float g0 = acos(dot(-n0, n1));
	const float g1 = acos(dot(-n1, n2));
	const float g2 = acos(dot(-n2, n3));
	const float g3 = acos(dot(-n3, n0));

	const float solidAngle = g0 + g1 + g2 + g3 - 2 * 3.14159265359;

	const float NdotL = solidAngle * 0.2 * (
		saturate(dot(normalize(L0), normal)) +
		saturate(dot(normalize(L1), normal)) +
		saturate(dot(normalize(L2), normal)) +
		saturate(dot(normalize(L3), normal)) +
		saturate(dot(normalize(light.Center - pos), normal)));

	const float3 r = reflect(-toEye, normal);
	const float3 intersectPoint = CalcPlaneIntersection(pos, r, light.Direction, light.Center);

	const float3 intersectionVector = intersectPoint - light.Center;
	const float2 intersectPlanePoint = float2(dot(intersectionVector, light.Right), dot(intersectionVector, light.Up));
	const float2 nearest2DPoint = float2(clamp(intersectPlanePoint.x, -light.Size.x, light.Size.x), clamp(intersectPlanePoint.y, -light.Size.y, light.Size.y));

	float3 specularFactor = 0;
	const float specularAmount = dot(r, light.Direction);
	if (specularAmount > 0) {
		const float specFactor = 1 - clamp(length(nearest2DPoint - intersectPlanePoint) * pow(mat.Shininess, 2) * 32, 0, 1);
		specularFactor += 0.7/**/ * specFactor * specularAmount * NdotL;
	}

	const float3 nearestPoint = light.Center + (light.Right * nearest2DPoint.x + light.Up * nearest2DPoint.y);
	const float dist = distance(pos, nearestPoint);
	const float falloff = 1 - saturate(dist / light.AttenuationRadius);

	const float3 lightVec = nearestPoint - pos;
	
	const float3 Li = light.Color * light.Intensity * falloff * (specularFactor/**/);

#if defined(BLINN_PHONG)
	return 0;
#elif defined(COOK_TORRANCE)
	return CookTorrance(mat, Li, lightVec, normal, toEye);
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
		else if (light.Type == LightType::E_Tube) result += ComputeTubeLight(light, mat, pos, normal, toEye);
		else if (light.Type == LightType::E_Rect) result += ComputeRectLight(light, mat, pos, normal, toEye);
	}

	return result;
}

#endif // __LIGHTINGUTIL_HLSLI__