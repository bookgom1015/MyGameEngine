#ifndef __SHADOWRAY_HLSL__
#define __SHADOWRAY_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"
#include "DxrShadingHelpers.hlsli"

struct ShadowHitInfo {
	bool IsHit;
};

ConstantBuffer<PassConstants> cb_Pass		: register(b0);

RaytracingAccelerationStructure				gi_BVH		: register(t0);
Texture2D<GBuffer::PositionMapFormat>		gi_Position	: register(t1);
Texture2D<GBuffer::NormalMapFormat>			gi_Normal	: register(t2);
Texture2D<GBuffer::DepthMapFormat>			gi_Depth	: register(t3);
RWTexture2D<DxrShadowMap::ShadowMapFormat>	gi_Shadow	: register(u0);

[shader("raygeneration")]
void ShadowRayGen() {
	uint2 launchIndex = DispatchRaysIndex().xy;

	float2 size;
	gi_Normal.GetDimensions(size.x, size.y);

	float3 normal = gi_Normal[launchIndex].xyz;
	float d = gi_Depth[launchIndex];

	if (d !=  GBuffer::InvalidDepthValue) {
		float3 posW = gi_Position[launchIndex].xyz;

		RayDesc ray;
		ray.Origin = posW + 0.1 * normal;
		ray.Direction = -cb_Pass.Lights[0].Direction;
		ray.TMin = 0;
		ray.TMax = 1000;

		ShadowHitInfo payload;
		payload.IsHit = false;

		TraceRay(
			gi_BVH,
			0,
			0xFF,
			0,
			1,
			0,
			ray,
			payload
		);

		float shadowFactor = payload.IsHit ? 0 : 1;
		float4 color = float4((float3)shadowFactor, 1);

		gi_Shadow[launchIndex] = shadowFactor;

		return;
	}

	gi_Shadow[launchIndex] = 1;
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowHitInfo payload, Attributes attrib) {
	payload.IsHit = true;
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo payload) {
	payload.IsHit = false;
}

#endif // __SHADOWRAY_HLSL__