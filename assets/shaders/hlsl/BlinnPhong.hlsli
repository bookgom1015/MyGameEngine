#ifndef __BLINNPHONG_HLSLI__
#define __BLINNPHONG_HLSLI__

float3 BlinnPhong(Material mat, float3 Li, float3 L, float3 N, float3 V, float NoL) {
	const float M = mat.Shininess * 256;

	const float3 H = normalize(V + L);
	const float NdotH = max(dot(H, N), 0);

	const float specular = (M + 8) * pow(max(dot(H, N), 0), M) / 8;

	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float3 diffuse = mat.Albedo.rgb;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1 - mat.Metalic);

	return (kD * diffuse / PI + kS * specular) * Li * NoL;
}

#endif // __BLINNPHONG_HLSLI__