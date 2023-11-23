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

RaytracingAccelerationStructure	gi_BVH		: register(t0);
Texture2D<float>				gi_Depth	: register(t1);
RWTexture2D<float>				gi_Shadow	: register(u0);

[shader("raygeneration")]
void ShadowRayGen() {
	float width, height;
	gi_Depth.GetDimensions(width, height);

	uint2 launchIndex = DispatchRaysIndex().xy;
	float d = gi_Depth[launchIndex];

	if (d < 1.0f) {
		float2 tex = float2((launchIndex.x + 0.5f) / width, (launchIndex.y + 0.5f) / height);
		float4 posH = float4(tex.x * 2.0f - 1.0f, (1.0f - tex.y) * 2.0f - 1.0f, 0.0f, 1.0f);
		float4 posV = mul(posH, cb_Pass.InvProj);
		posV /= posV.w;

		float dv = NdcDepthToViewDepth(d, cb_Pass.Proj);
		posV = (dv / posV.z) * posV;

		float4 posW = mul(float4(posV.xyz, 1.0f), cb_Pass.InvView);

		RayDesc ray;
		ray.Origin = posW.xyz;
		ray.Direction = -cb_Pass.Lights[0].Direction;
		ray.TMin = 0.001f;
		ray.TMax = 1000.0f;

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

		float shadowFactor = payload.IsHit ? 0.0f : 1.0f;
		float4 color = float4((float3)shadowFactor, 1.0f);

		gi_Shadow[launchIndex] = shadowFactor;

		return;
	}

	gi_Shadow[launchIndex] = 1.0f;
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