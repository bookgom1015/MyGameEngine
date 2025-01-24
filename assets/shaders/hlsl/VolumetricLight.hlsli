#ifndef __VOLUMETRICLIGHT_HLSLI__
#define __VOLUMETRICLIGHT_HLSLI__

#include "ShadingConstants.hlsli"

float HenyeyGreensteinPhaseFunction(float3 wi, float3 wo, float g /* Anisotropy coefficient */) {
	float theta = dot(wi, wo);
	float g2 = g * g;
	float denom = pow(1 + g2 - 2 * g * theta, 3 / 2);
	return (1 / (4 * PI)) * ((1 - g2) / max(denom, FLT_EPSILON));
}

#endif // __VOLUMETRICLIGHT_HLSLI__