// [ Dependencies ]
//  - ShadingHelpers.hlsli
//
// [ References ]
//  - https://xtozero.tistory.com/5
//   -- https://github.com/diharaw/volumetric-fog
//   -- https://github.com/bartwronski/CSharpRenderer
//   -- https://github.com/Unity-Technologies/VolumetricLighting/tree/master/Assets/VolumetricFog

#ifndef __VOLUMETRICLIGHT_HLSLI__
#define __VOLUMETRICLIGHT_HLSLI__

// [ Parameters ]
//  - wi : the direction which light comes in
//  - wo : the direction which light goes out
//  - g  : anisotropic coefficient
//
// [ Return ]
//  - Amount of 1ight leaving
float HenyeyGreensteinPhaseFunction(float3 wi, float3 wo, float g) {
	float theta = dot(wi, wo);
	float g2 = g * g;
	float denom = pow(1 + g2 - 2 * g * theta, 3 / 2);
	return (1 / (4 * PI)) * ((1 - g2) / max(denom, FLT_EPSILON));
}

float SliceThickness(float z_ndc, float z_exp, float near, float far, uint dimZ) {
	float currDepthV = NdcDepthToExpViewDepth(z_ndc, z_exp, near, far);
	float nextDepthV = NdcDepthToExpViewDepth(z_ndc + 1 / (float)dimZ, z_exp, near, far);
	return nextDepthV - currDepthV;
}

// [ Parameters ]
//  - accumLight		 : accumlated lights
//  - accumTransmittance : accumlated transmittance
//  - sliceLight		 : light of current position
//  - sliceDensity		 : density of current position
//  - thickness			 : difference between current slice's depth and next slice's depth
//  - densityScale		 : scale for density(a number appropriately adjusted to use a larger value
//							for the uniform density parameter rather than a decimal point 
//
// [ Return ]
//  - Accumlated lights(rgb) and transmittance(a)
float4 ScatterStep(
		inout float3 accumLight, 
		inout float accumTransmittance, 
		in float3 sliceLight,
		inout float sliceDensity, 
		in float thickness,
		in float densityScale) {
	sliceDensity = max(sliceDensity, 0.0000001);
	sliceDensity *= densityScale;
	const float sliceTransmittance = exp(-sliceDensity * thickness);

	// The equation used in Frostbite
	const float3 sliceLightIntegral = sliceLight * (1 - sliceTransmittance) / sliceDensity;

	accumLight += sliceLightIntegral * accumTransmittance;
	accumTransmittance *= sliceTransmittance;

	return float4(accumLight, accumTransmittance);
}

#endif // __VOLUMETRICLIGHT_HLSLI__