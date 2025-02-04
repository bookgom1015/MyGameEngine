#ifndef __ACCUMULATESCATTERING_HLSL__
#define __ACCUMULATESCATTERING_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

#include "VolumetricLight.hlsli"

cbuffer cbRootConstants : register (b0) {
	float gNearZ;
	float gFarZ;
	float gDepthExponent;
	float gDensityScale;
}

RWTexture3D<VolumetricLight::FrustumMapFormat>	gio_FrustumVolume	: register(u0);

[numthreads(
	VolumetricLight::AccumulateScattering::ThreadGroup::Width,
	VolumetricLight::AccumulateScattering::ThreadGroup::Height,
	1)]
void CS(uint3 DTid : SV_DispatchThreadID) {
	uint3 dims;
	gio_FrustumVolume.GetDimensions(dims.x, dims.y, dims.z);
	if (all(DTid >= dims)) return;

	float4 accum = float4(0.f, 0.f, 0.f, 1.f);
	uint3 pos = uint3(DTid.xy, 0.f);

	[loop]
	for (uint z = 0; z < dims.z; ++z) {
		pos.z = z;
		const float4 slice = gio_FrustumVolume[pos];
		const float tickness = SliceTickness((float)z / dims.z, gDepthExponent, gNearZ, gFarZ, dims.z);

		accum = ScatterStep(accum.rgb, accum.a, slice.rgb, slice.a, tickness, gDensityScale);

		gio_FrustumVolume[pos] = accum;
	}
}

#endif // __ACCUMULATESCATTERING_HLSL__