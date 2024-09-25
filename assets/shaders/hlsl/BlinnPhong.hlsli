#ifndef __BLINNPHONG_HLSLI__
#define __BLINNPHONG_HLSLI__

#include "ShadingHelpers.hlsli"

float3 BlinnPhong(Material mat, float3 luminosity, float3 L, float3 N, float3 V) {
	const float3 lightDir = normalize(L);
	const float M = mat.Shininess * 256;

	const float3 H = normalize(V + lightDir);
	const float NdotH = max(dot(H, N), 0);

	const float specular = (M + 8) * pow(max(dot(H, N), 0), M) / 8;

	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float3 diffuse = mat.Albedo.rgb;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1 - mat.Metalic);

	float NdotL = max(dot(N, lightDir), 0);

	return (kD * diffuse / PI + kS * specular) * luminosity * NdotL;
}

#endif // __BLINNPHONG_HLSLI__