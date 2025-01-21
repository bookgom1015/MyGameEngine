#ifndef __BRDF_HLSLI__
#define __BRDF_HLSLI__

#include "ShadingConstants.hlsli"

// Trowbridge-Reitz GGX 
float DistributionGGX(float3 N, float3 H, float roughness) {
	const float a = roughness * roughness;
	const float a2 = a * a;
	const float NdotH = max(dot(N, H), 0);
	const float NdotH2 = NdotH * NdotH;

	const float num = a2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return num / denom;
}

float DistributionGGX_Modified(float3 N, float3 H, float roughness, float d, float radius) {
	const float a = roughness * roughness;
	const float a_ = saturate(radius / (2 * d) + a);
	const float a2 = a * a;
	const float a_2 = a_ * a_;
	const float NdotH = max(dot(N, H), 0);
	const float NdotH2 = NdotH * NdotH;

	const float num = a2 * a_2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return num / denom;
}

// Smith's method with Schlick-GGX 
// 
// k is a remapping of ес based on whether using the geometry function 
//  for either direct lighting or IBL lighting.
float GeometryShlickGGX(float NdotV, float roughness) {
	const float a = (roughness + 1);
	const float k = (a * a) / 8;

	const float num = NdotV;
	const float denom = NdotV * (1 - k) + k;

	return num / denom;
}

float GeometryShlickGGX_IBL(float NdotV, float roughness) {
	const float a = roughness;
	const float k = (a * a) / 2;

	const float num = NdotV;
	const float denom = NdotV * (1 - k) + k;

	return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
	const float NdotV = max(dot(N, V), 0);
	const float NdotL = max(dot(N, L), 0);

	const float ggx1 = GeometryShlickGGX(NdotV, roughness);
	const float ggx2 = GeometryShlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness) {
	const float NdotV = max(dot(N, V), 0);
	const float NdotL = max(dot(N, L), 0);

	const float ggx1 = GeometryShlickGGX_IBL(NdotV, roughness);
	const float ggx2 = GeometryShlickGGX_IBL(NdotL, roughness);

	return ggx1 * ggx2;
}

// the Fresnel Schlick approximation
float3 FresnelSchlick(float cos, float3 F0) {
	return F0 + (1 - F0) * pow(max((1 - cos), 0), 5);
}

float3 FresnelSchlickRoughness(float cos, float3 F0, float roughness) {
	return F0 + (max((float3)(1 - roughness), F0) - F0) * pow(max(1 - cos, 0), 5);
}

float RadicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N) {
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness) {
	float a = roughness * roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

#include "BlinnPhong.hlsli"
#include "CookTorrance.hlsli"

#endif // __BRDF_HLSLI__