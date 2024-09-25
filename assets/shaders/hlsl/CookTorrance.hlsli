#ifndef __COOKTORRANCE_HLSLI__
#define __COOKTORRANCE_HLSLI__

#include "ShadingHelpers.hlsli"

float3 CookTorrance(Material mat, float3 luminosity, float3 L, float3 N, float3 V) {
	const float3 lightDir = normalize(L);

	const float3 H = normalize(V + lightDir);
	const float roughness = 1 - mat.Shininess;

	const float NDF = DistributionGGX(N, H, roughness);
	const float G = GeometrySmith(N, V, lightDir, roughness);
	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float NdotL = max(dot(N, lightDir), 0);

	const float3 diffuse = mat.Albedo.rgb;

	const float3 numerator = NDF * G * F;
	const float denominator = 4 * max(dot(N, V), 0) * NdotL + 0.0001;
	const float3 specular = numerator / denominator;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1 - mat.Metalic);

	return (kD * diffuse / PI + specular) * luminosity * NdotL;
}

float3 CookTorrance_Bulb(Material mat, float3 luminosity, float3 L, float3 N, float3 V, float3 pos, float radius) {
	const float d = length(L);
	const float3 r = reflect(-V, N);
	const float3 centerToRay = dot(L, r) * r - L;
	const float3 closestPoint = L + centerToRay * saturate(radius / length(centerToRay));
	const float3 l = normalize(closestPoint);
	const float3 H = normalize(V + l);
	const float roughness = 1 - mat.Shininess;

	const float NDF = DistributionGGX_Modified(N, H, roughness, d, radius);
	const float G = GeometrySmith(N, V, l, roughness);
	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float NdotL = max(dot(N, l), 0);

	const float3 diffuse = mat.Albedo.rgb;

	const float3 numerator = NDF * G * F;
	const float denominator = 4 * max(dot(N, V), 0) * NdotL + 0.0001;
	const float3 specular = numerator / denominator;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1 - mat.Metalic);

	return (kD * (diffuse / PI) + specular) * luminosity * NdotL;
}

#endif // __COOKTORRANCE_HLSLI__