#ifndef __COOKTORRANCE_HLSLI__
#define __COOKTORRANCE_HLSLI__

#include "ShadingHelpers.hlsli"

//
// Trowbridge-Reitz GGX 
// 
float DistributionGGX(float3 normal, float3 halfV, float roughness) {
	float a2 = roughness * roughness;
	float NdotH = max(dot(normal, halfV), 0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return nom / denom;
}

//
// Smith's method with Schlick-GGX 
// 
// k is a remapping of ес based on whether using the geometry function 
//  for either direct lighting or IBL lighting.
float GeometryShlickGGX(float NdotV, float k) {
	float nom = NdotV;
	float denom = NdotV * (1 - k) + k;

	return nom / denom;
}

float GeometrySmith(float3 normal, float3 view, float3 light, float k) {
	float NdotV = max(dot(normal, view), 0);
	float NdotL = max(dot(normal, light), 0);

	float ggx1 = GeometryShlickGGX(NdotV, k);
	float ggx2 = GeometryShlickGGX(NdotL, k);

	return ggx1 * ggx2;
}

//
// the Fresnel Schlick approximation
//
float3 FresnelShlick(float cos, float3 F0) {
	return F0 + (1 - F0) * pow(1 - cos, 5);
}

#endif // __COOKTORRANCE_HLSLI__