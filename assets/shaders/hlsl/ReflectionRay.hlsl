#ifndef __REFLECTIONRAY_HLSL__
#define __REFLECTIONRAY_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "DxrShadingHelpers.hlsli"
#include "Samplers.hlsli"

typedef BuiltInTriangleIntersectionAttributes Attributes;

ConstantBuffer<RaytracedReflectionConstantBuffer> cbRr : register(b0);

RaytracingAccelerationStructure				gBVH			: register(t0);
Texture2D<HDR_FORMAT>						gi_BackBuffer	: register(t1);
Texture2D<GBuffer::NormalDepthMapFormat>	gi_NormalDepth	: register(t2);

RWTexture2D<float4>	go_Reflection	: register(u0);

struct Ray {
	float3 Origin;
	float3 Direction;
};

struct RayPayload {
	float4 Irrad;
};

[shader("raygeneration")]
void ReflectionRayGen() {
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 texc = (launchIndex + 0.5) / cbRr.TextureDim;

	float3 normal;
	float depth;
	DecodeNormalDepth(gi_NormalDepth[launchIndex], normal, depth);
	
	if (depth == GBuffer::RayHitDistanceOnMiss) {
		go_Reflection[launchIndex] = 0;
		return;
	}

	float3 hitPosition;
	CalculateHitPosition(cbRr.Proj, cbRr.InvProj, cbRr.InvView, cbRr.TextureDim, depth, launchIndex, hitPosition);

	float3 fromEye = normalize(hitPosition - cbRr.EyePosW);
	float3 toLight = reflect(fromEye, normal);

	RayDesc ray;
	ray.Origin = hitPosition + 0.01 * normal;
	ray.Direction = toLight;
	ray.TMin = 0;
	ray.TMax = cbRr.ReflectionRadius;

	RayPayload payload = { (float4)0 };

	TraceRay(
		gBVH,
		RAY_FLAG_CULL_FRONT_FACING_TRIANGLES,
		0xFF,
		0,
		0,
		0,
		ray,
		payload
	);

	go_Reflection[launchIndex] = payload.Irrad;
}

[shader("closesthit")]
void ReflectionClosestHit(inout RayPayload payload, Attributes attrib) {
	payload.Irrad = 1;
}

[shader("miss")]
void ReflectionMiss(inout RayPayload payload) {
	payload.Irrad = 0;
}	

#endif // __REFLECTIONRAY_HLSL__