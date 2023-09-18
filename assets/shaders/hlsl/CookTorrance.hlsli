#ifndef __COOKTORRANCE_HLSLI__
#define __COOKTORRANCE_HLSLI__

#include "ShadingHelpers.hlsli"

//
// Trowbridge-Reitz GGX 
// 
float DistributionGGX(float3 N, float3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return num / denom;
}

//
// Smith's method with Schlick-GGX 
// 
// k is a remapping of ес based on whether using the geometry function 
//  for either direct lighting or IBL lighting.
float GeometryShlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1);
	float k = (r * r) / 8;

	float num = NdotV;
	float denom = NdotV * (1 - k) + k;

	return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
	float NdotV = max(dot(N, V), 0);
	float NdotL = max(dot(N, L), 0);

	float ggx1 = GeometryShlickGGX(NdotV, roughness);
	float ggx2 = GeometryShlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float3 CookTorrance(float3 radiance, float3 L, float3 N, float3 V, Material mat) {
	const float3 H = normalize(V + L);
	const float roughness = 1 - mat.Shininess;

	const float NDF = DistributionGGX(N, H, roughness);
	const float G = GeometrySmith(N, V, L, roughness);
	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float NdotL = max(dot(N, L), 0);

	const float3 diffuse = mat.Albedo.rgb;

	const float3 numerator = NDF * G * F;
	const float denominator = 4 * max(dot(N, V), 0) * NdotL + 0.0001;
	const float3 specular = numerator / denominator;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1 - mat.Metalic);

	return (kD * diffuse / PI + specular) * radiance * NdotL;
}

#endif // __COOKTORRANCE_HLSLI__