#ifndef __BRDF_HLSLI__
#define __BRDF_HLSLI__

#include "ShadingConstants.hlsli"

// Trowbridge-Reitz GGX 
float DistributionGGX(in float3 N, in float3 H, in float roughness) {
	const float a = roughness * roughness;
	const float a2 = a * a;
	const float NdotH = max(dot(N, H), 0.f);
	const float NdotH2 = NdotH * NdotH;

	const float num = a2;
	float denom = (NdotH2 * (a2 - 1.f) + 1.f);
	denom = PI * denom * denom;

	return num / denom;
}

float DistributionGGX_Modified(in float3 N, in float3 H, in float roughness, in float d, in float radius) {
	const float a = roughness * roughness;
	const float a_ = saturate(radius / (2.f * d) + a);
	const float a2 = a * a;
	const float a_2 = a_ * a_;
	const float NdotH = max(dot(N, H), 0.f);
	const float NdotH2 = NdotH * NdotH;

	const float num = a2 * a_2;
	float denom = (NdotH2 * (a2 - 1.f) + 1.f);
	denom = PI * denom * denom;

	return num / denom;
}

// Smith's method with Schlick-GGX 
// 
// k is a remapping of ес based on whether using the geometry function 
//  for either direct lighting or IBL lighting.
float GeometryShlickGGX(in float NdotV, in float roughness) {
	const float a = (roughness + 1.f);
	const float k = (a * a) / 8.f;

	const float num = NdotV;
	const float denom = NdotV * (1.f - k) + k;

	return num / denom;
}

float GeometryShlickGGX_IBL(in float NdotV, in float roughness) {
	const float a = roughness;
	const float k = (a * a) / 2.f;

	const float num = NdotV;
	const float denom = NdotV * (1.f - k) + k;

	return num / denom;
}

float GeometrySmith(in float3 N, in float3 V, in float3 L, in float roughness) {
	const float NdotV = max(dot(N, V), 0.f);
	const float NdotL = max(dot(N, L), 0.f);

	const float ggx1 = GeometryShlickGGX(NdotV, roughness);
	const float ggx2 = GeometryShlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float GeometrySmith_IBL(in float3 N, in float3 V, in float3 L, in float roughness) {
	const float NdotV = max(dot(N, V), 0.f);
	const float NdotL = max(dot(N, L), 0.f);

	const float ggx1 = GeometryShlickGGX_IBL(NdotV, roughness);
	const float ggx2 = GeometryShlickGGX_IBL(NdotL, roughness);

	return ggx1 * ggx2;
}

// the Fresnel Schlick approximation
float3 FresnelSchlick(in float cos, in float3 F0) {
	return F0 + (1.f - F0) * pow(max((1.f - cos), 0.f), 5.f);
}

float3 FresnelSchlickRoughness(in float cos, in float3 F0, in float roughness) {
	return F0 + (max((float3)(1.f - roughness), F0) - F0) * pow(max(1.f - cos, 0.f), 5.f);
}

float RadicalInverse_VdC(in uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(in uint i, in uint N) {
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(in float2 Xi, in float3 N, in float roughness) {
	const float a = roughness * roughness;

	const float phi = 2.f * PI * Xi.x;
	const float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y));
	const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	const float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	const float3 tangent = normalize(cross(up, N));
	const float3 bitangent = cross(N, tangent);

	const float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

#include "BlinnPhong.hlsli"
#include "CookTorrance.hlsli"

#endif // __BRDF_HLSLI__