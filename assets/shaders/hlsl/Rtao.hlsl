#ifndef __RTAO_HLSL__
#define __RTAO_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "RandGenerator.hlsli"
#include "Samplers.hlsli"
#include "Rtao.hlsli"

typedef BuiltInTriangleIntersectionAttributes Attributes;

struct Ray {
	float3 Origin;
	float3 Direction;
};

struct RayPayload {
	float tHit;
};

ConstantBuffer<RtaoConstants> cbRtao : register(b0);

cbuffer cbRootConstants : register(b1) {
	uint2 gTextureDim;
};

// Nonnumeric values cannot be added to a cbuffer.
RaytracingAccelerationStructure				gBVH			: register(t0);
Texture2D<GBuffer::NormalDepthMapFormat>	gi_NormalDepth	: register(t1);
Texture2D<GBuffer::DepthMapFormat>			gi_DepthMap		: register(t2);

RWTexture2D<float> go_AOCoefficient		: register(u0);
RWTexture2D<float> go_RayHitDistance	: register(u1);

// Generates a seed for a random number generator from 2 inputs plus a backoff
uint InitRand(uint val0, uint val1, uint backoff = 16) {
	uint v0 = val0;
	uint v1 = val1;
	uint s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; ++n) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

bool TraceAORayAndReportIfHit(out float tHit, Ray aoRay, float TMax, float3 surfaceNormal) {
	RayDesc ray;
	// Nudge the origin along the surface normal a bit to avoid starting from
	//  behind the surface due to float calculations imprecision.
	ray.Origin = aoRay.Origin + 0.01 * surfaceNormal;
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

	return Rtao::HasAORayHitAnyGeometry(tHit);
}

float CalculateAO(out float tHit, uint2 launchIndex, Ray aoRay, float3 surfaceNormal) {
	float occlusion = 0;
	const float TMax = cbRtao.OcclusionRadius;
	if (TraceAORayAndReportIfHit(tHit, aoRay, TMax, surfaceNormal)) {
		float3 hitPosition = aoRay.Origin + tHit * aoRay.Direction;
		float distZ = distance(aoRay.Origin, hitPosition);
		occlusion = OcclusionFunction(distZ, cbRtao.SurfaceEpsilon, cbRtao.OcclusionFadeStart, cbRtao.OcclusionFadeEnd);
	}
	return occlusion;
}

[shader("raygeneration")]
void RtaoRayGen() {
	uint2 launchIndex = DispatchRaysIndex().xy;

	float3 surfaceNormal;
	float depth;
	DecodeNormalDepth(gi_NormalDepth[launchIndex], surfaceNormal, depth);

	float tHit = Rtao::RayHitDistanceOnMiss;
	float ambientCoef = Rtao::InvalidAOCoefficientValue;

	if (depth != Rtao::RayHitDistanceOnMiss) {
		float3 hitPosition;
		CalculateHitPosition(cbRtao.Proj, cbRtao.InvProj, cbRtao.InvView, gTextureDim, depth, launchIndex, hitPosition);

		uint seed = InitRand(launchIndex.x + launchIndex.y * gTextureDim.x, cbRtao.FrameCount);

		float3 direction = CosHemisphereSample(seed, surfaceNormal);
		float flip = sign(dot(direction, surfaceNormal));
		direction = flip * direction;

		float occlusionSum = 0;

		for (int i = 0; i < cbRtao.SampleCount; ++i) {
			Ray aoRay = { hitPosition, direction };
			occlusionSum += CalculateAO(tHit, launchIndex, aoRay, surfaceNormal);
		}

		occlusionSum /= cbRtao.SampleCount;
		ambientCoef = 1 - occlusionSum;
	}

	go_AOCoefficient[launchIndex] = ambientCoef;
	go_RayHitDistance[launchIndex] = Rtao::HasAORayHitAnyGeometry(tHit) ? tHit : cbRtao.OcclusionRadius;
}

[shader("closesthit")]
void RtaoClosestHit(inout RayPayload payload, Attributes attrib) {
	payload.tHit = RayTCurrent();
}

[shader("miss")]
void RtaoMiss(inout RayPayload payload) {
	payload.tHit = Rtao::RayHitDistanceOnMiss;
}

#endif // __RTAO_HLSL__