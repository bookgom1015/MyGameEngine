#ifndef __PHASEFUNCTION_HLSLI__
#define __PHASEFUNCTION_HLSLI__

#include "ShadingHelpers.hlsli"

float HenyeyGreensteinPhaseFunction(float3 wi, float3 wo, float g /* Anisotropy coefficient */) {
	float theta = dot(wi, wo);
	float g2 = g * g;
	float denom = pow(1 + g2 - 2 * g * theta, 3 / 2);
	return (1 / (4 * PI)) * ((1 - g2) / max(denom, FLT_EPSILON));
}

#endif // __PHASEFUNCTION_HLSLI__