#ifndef __COOKTORRANCE_HLSLI__
#define __COOKTORRANCE_HLSLI__

float3 CookTorrance(Material mat, float3 Li, float3 L, float3 N, float3 V, float NoL) {
	const float3 H = normalize(V + L);
	const float roughness = 1 - mat.Shininess;

	const float NDF = DistributionGGX(N, H, roughness);
	const float G = GeometrySmith(N, V, L, roughness);
	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float3 diffuse = mat.Albedo.rgb / PI;

	const float3 numerator = NDF * G * F;
	const float denominator = 4 * max(dot(N, V), 0) * NoL + 0.0001;
	const float3 specular = numerator / denominator;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1 - mat.Metalic);

	return (kD * diffuse + specular) * Li * NoL;
}

float3 CookTorrance_GGXModified(Material mat, float3 Li, float3 L, float3 N, float3 V, float NoL, float d, float radius) {
	const float3 H = normalize(V + L);
	const float roughness = 1 - mat.Shininess;

	const float NDF = DistributionGGX_Modified(N, H, roughness, d, radius);
	const float G = GeometrySmith(N, V, L, roughness);
	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float3 diffuse = mat.Albedo.rgb / PI;

	const float3 numerator = NDF * G * F;
	const float denominator = 4 * max(dot(N, V), 0) * NoL + 0.0001;
	const float3 specular = numerator / denominator;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1 - mat.Metalic);

	return (kD * diffuse + specular) * Li * NoL;
}

#endif // __COOKTORRANCE_HLSLI__