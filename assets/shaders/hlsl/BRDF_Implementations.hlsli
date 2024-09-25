#ifndef __BRDF_IMPLEMENTATIONS_HLSLI__
#define __BRDF_IMPLEMENTATIONS_HLSLI__

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
	const float a_ = saturate(a + radius / 2 * d);
	const float a2 = a * a;
	const float a_2 = a_ * a_;
	const float NdotH = max(dot(N, H), 0);
	const float NdotH2 = NdotH * NdotH;

	const float num = a2 * a_2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return num / denom;
}

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

float3 FresnelSchlick(float cos, float3 F0) {
	return F0 + (1 - F0) * pow(max((1 - cos), 0), 5);
}

float3 FresnelSchlickRoughness(float cos, float3 F0, float roughness) {
	return F0 + (max((float3)(1 - roughness), F0) - F0) * pow(max(1 - cos, 0), 5);
}

#endif // __BRDF_IMPLEMENTATIONS_HLSLI__