#ifndef __CALCULATEPARTIALDERIVATIVECS_HLSL__
#define __CALCULATEPARTIALDERIVATIVECS_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

#include "SVGF.hlsli"

cbuffer cbRootConstants : register (b0) {
	float2 gInvTextureDim;
}

Texture2D<Shadow::ZDepthMapFormat>					gi_Depth					: register(t0);
RWTexture2D<SVGF::DepthPartialDerivativeMapFormat>	go_DepthPartialDerivative	: register(u0);

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 DTid : SV_DispatchThreadID) {
	float2 tex = (DTid + 0.5) * gInvTextureDim;

	float top	 = gi_Depth.SampleLevel(gsamPointClamp, tex + float2(0, -gInvTextureDim.y), 0);
	float bottom = gi_Depth.SampleLevel(gsamPointClamp, tex + float2(0,  gInvTextureDim.y), 0);
	float left	 = gi_Depth.SampleLevel(gsamPointClamp, tex + float2(-gInvTextureDim.x, 0), 0);
	float right	 = gi_Depth.SampleLevel(gsamPointClamp, tex + float2( gInvTextureDim.x, 0), 0);

	float center = gi_Depth.SampleLevel(gsamPointClamp, tex, 0);
	float2 backwardDiff = center - float2(left, top);
	float2 forwardDiff = float2(right, bottom) - center;

	// Calculates partial derivatives as the min of absolute backward and forward differences.

	// Find the absolute minimum of the backward and foward differences in each axis
    // while preserving the sign of the difference.
	float2 ddx = float2(backwardDiff.x, forwardDiff.x);
	float2 ddy = float2(backwardDiff.y, forwardDiff.y);

	uint2 minIndex = {
		GetIndexOfValueClosestToReference(0, ddx),
		GetIndexOfValueClosestToReference(0, ddy)
	};
	float2 ddxy = float2(ddx[minIndex.x], ddy[minIndex.y]);

	// Clamp ddxy to a reasonable value to avoid ddxy going over surface boundaries
	// on thin geometry and getting background/foreground blended together on blur.
	float maxDdxy = 1;
	float2 _sign = sign(ddxy);
	ddxy = _sign * min(abs(ddxy), maxDdxy);

	go_DepthPartialDerivative[DTid] = ddxy;
}

#endif // __CALCULATEPARTIALDERIVATIVECS_HLSL__