// [ References ]
//  - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingRealTimeDenoisedAmbientOcclusion

#ifndef __RTAO_HLSL__
#define __RTAO_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

#include "DXR_ShadingHelpers.hlsli"
#include "ValuePackaging.hlsli"
#include "Random.hlsli"

typedef BuiltInTriangleIntersectionAttributes Attributes;

struct Ray {
	float3 Origin;
	float3 Direction;
};

struct RayPayload {
	float tHit;
};

ConstantBuffer<ConstantBuffer_RTAO> cb_RTAO : register(b0);

// Nonnumeric values cannot be added to a cbuffer.
RaytracingAccelerationStructure				gBVH			: register(t0);
Texture2D<GBuffer::PositionMapFormat>		gi_Position		: register(t1);
Texture2D<GBuffer::NormalDepthMapFormat>	gi_NormalDepth	: register(t2);
Texture2D<GBuffer::DepthMapFormat>			gi_DepthMap		: register(t3);

RWTexture2D<float> go_AOCoefficient		: register(u0);
RWTexture2D<float> go_RayHitDistance	: register(u1);

bool TraceAORayAndReportIfHit(out float tHit, Ray aoRay, float TMax, float3 surfaceNormal) {
	RayDesc ray;
	// Nudge the origin along the surface normal a bit to avoid starting from
	//  behind the surface due to float calculations imprecision.
	ray.Origin = aoRay.Origin + 0.1 * surfaceNormal;
	ray.Direction = aoRay.Direction;
	ray.TMin = 0;
	ray.TMax = TMax;

	RayPayload payload = { TMax };

	TraceRay(
		gBVH,
		0,
		0xFF,
		0,
		0,
		0,
		ray,
		payload
	);

	tHit = payload.tHit;

	return RTAO::HasAORayHitAnyGeometry(tHit);
}

float CalculateAO(out float tHit, uint2 launchIndex, Ray aoRay, float3 surfaceNormal) {
	float occlusion = 0;
	const float TMax = cb_RTAO.OcclusionRadius;
	if (TraceAORayAndReportIfHit(tHit, aoRay, TMax, surfaceNormal)) {
		float3 hitPosition = aoRay.Origin + tHit * aoRay.Direction;
		float distZ = distance(aoRay.Origin, hitPosition);
		occlusion = OcclusionFunction(distZ, cb_RTAO.SurfaceEpsilon, cb_RTAO.OcclusionFadeStart, cb_RTAO.OcclusionFadeEnd);
	}
	return occlusion;
}

[shader("raygeneration")]
void RTAO_RayGen() {
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 dimensions = DispatchRaysDimensions().xy;

	float3 surfaceNormal;
	float depth;
	DecodeNormalDepth(gi_NormalDepth[launchIndex], surfaceNormal, depth);

	float tHit = RTAO::RayHitDistanceOnMiss;
	float ambientCoef = RTAO::InvalidAOCoefficientValue;

	if (depth != RTAO::RayHitDistanceOnMiss) {
		float3 hitPosition = gi_Position[launchIndex].xyz;

		uint seed = InitRand(launchIndex.x + launchIndex.y * dimensions.x, cb_RTAO.FrameCount);

		float3 direction = CosHemisphereSample(seed, surfaceNormal);
		float flip = sign(dot(direction, surfaceNormal));
		direction = flip * direction;

		float occlusionSum = 0;

		for (int i = 0; i < cb_RTAO.SampleCount; ++i) {
			Ray aoRay = { hitPosition, direction };
			occlusionSum += CalculateAO(tHit, launchIndex, aoRay, surfaceNormal);
		}

		occlusionSum /= cb_RTAO.SampleCount;
		ambientCoef = 1 - occlusionSum;
	}

	go_AOCoefficient[launchIndex] = ambientCoef;
	go_RayHitDistance[launchIndex] = RTAO::HasAORayHitAnyGeometry(tHit) ? tHit : cb_RTAO.OcclusionRadius;
}

[shader("closesthit")]
void RTAO_ClosestHit(inout RayPayload payload, Attributes attrib) {
	payload.tHit = RayTCurrent();
}

[shader("miss")]
void RTAO_Miss(inout RayPayload payload) {
	payload.tHit = RTAO::RayHitDistanceOnMiss;
}

#endif // __RTAO_HLSL__