#ifndef __COOKTORRANCE_HLSLI__
#define __COOKTORRANCE_HLSLI__

float3 CookTorrance(in Material mat, in float3 Li, in float3 L, in float3 N, in float3 V, in float NoL) {
	const float3 H = normalize(V + L);
	const float roughness = 1.f - mat.Shininess;

	const float NDF = DistributionGGX(N, H, roughness);
	const float G = GeometrySmith(N, V, L, roughness);
	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float3 diffuse = mat.Albedo.rgb / PI;

	const float3 numerator = NDF * G * F;
	const float denominator = 4.f * max(dot(N, V), 0.f) * NoL + 0.0001f;
	const float3 specular = numerator / denominator;

	const float3 kS = F;
	float3 kD = 1.f - kS;
	kD *= (1.f - mat.Metalic);

	return (kD * diffuse + specular) * Li * NoL;
}

float3 CookTorrance_GGXModified(in Material mat, in float3 Li, in float3 L, in float3 N, in float3 V, in float NoL, in float d, in float radius) {
	const float3 H = normalize(V + L);
	const float roughness = 1.f - mat.Shininess;

	const float NDF = DistributionGGX_Modified(N, H, roughness, d, radius);
	const float G = GeometrySmith(N, V, L, roughness);
	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float3 diffuse = mat.Albedo.rgb / PI;

	const float3 numerator = NDF * G * F;
	const float denominator = 4.f * max(dot(N, V), 0.f) * NoL + 0.0001f;
	const float3 specular = numerator / denominator;

	const float3 kS = F;
	float3 kD = 1.f - kS;
	kD *= (1.f - mat.Metalic);

	return (kD * diffuse + specular) * Li * NoL;
}

#endif // __COOKTORRANCE_HLSLI__