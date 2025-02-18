#ifndef __BLINNPHONG_HLSLI__
#define __BLINNPHONG_HLSLI__

float3 BlinnPhong(in Material mat, in float3 Li, in float3 L, in float3 N, in float3 V, in float NoL) {
	const float M = mat.Shininess * 256.f;

	const float3 H = normalize(V + L);
	const float NdotH = max(dot(H, N), 0.f);

	const float specular = (M + 8.f) * pow(max(dot(H, N), 0.f), M) / 8.f;

	const float3 F = FresnelSchlick(saturate(dot(H, V)), mat.FresnelR0);

	const float3 diffuse = mat.Albedo.rgb;

	const float3 kS = F;
	float3 kD = 1 - kS;
	kD *= (1.f - mat.Metalic);

	return (kD * diffuse / PI + kS * specular) * Li * NoL;
}

#endif // __BLINNPHONG_HLSLI__