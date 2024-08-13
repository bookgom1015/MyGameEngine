#ifndef __BRDF_IMPLEMENTATIONS_HLSLI__
#define __BRDF_IMPLEMENTATIONS_HLSLI__

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

float GeometryShlickGGX(float NdotV, float roughness) {
	float a = (roughness + 1);
	float k = (a * a) / 8;

	float num = NdotV;
	float denom = NdotV * (1 - k) + k;

	return num / denom;
}

float GeometryShlickGGX_IBL(float NdotV, float roughness) {
	float a = roughness;
	float k = (a * a) / 2;

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

float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness) {
	float NdotV = max(dot(N, V), 0);
	float NdotL = max(dot(N, L), 0);

	float ggx1 = GeometryShlickGGX_IBL(NdotV, roughness);
	float ggx2 = GeometryShlickGGX_IBL(NdotL, roughness);

	return ggx1 * ggx2;
}

float3 FresnelSchlick(float cos, float3 F0) {
	return F0 + (1 - F0) * pow(max((1 - cos), 0), 5);
}

float3 FresnelSchlickRoughness(float cos, float3 F0, float roughness) {
	return F0 + (max((float3)(1 - roughness), F0) - F0) * pow(max(1 - cos, 0), 5);
}

#endif // __BRDF_IMPLEMENTATIONS_HLSLI__