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

RaytracingAccelerationStructure				gi_BVH			: register(t0);
Texture2D<HDR_FORMAT>						gi_BackBuffer	: register(t1);
Texture2D<GBuffer::NormalDepthMapFormat>	gi_NormalDepth	: register(t2);
RWTexture2D<float4>							go_Reflection	: register(u0);

ConstantBuffer<ObjectConstants>		lcb_Obj	: register(b0, space1);
ConstantBuffer<MaterialConstants>	lcb_Mat	: register(b1, space1);

StructuredBuffer<Vertex>	lsb_Vertices	: register(t0, space1);
StructuredBuffer<uint>		lsb_Indices		: register(t1, space1);

struct RayPayload {
	float4	Irrad;
	bool	IsHit;
};

[shader("raygeneration")]
void RadianceRayGen() {
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 texc = (launchIndex + 0.5) / cbRr.TextureDim;

	float3 normal;
	float depth;
	DecodeNormalDepth(gi_NormalDepth[launchIndex], normal, depth);
	
	if (depth == GBuffer::InvalidNormDepthValue) {
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

	RayPayload payload = { (float4)0, false };

	//TraceRay(
	//	gi_BVH,
	//	0,
	//	RaytracedReflection::InstanceMask,
	//	RaytracedReflection::HitGroup::Offset[RaytracedReflection::Ray::E_Radiance],
	//	RaytracedReflection::HitGroup::GeometryStride,
	//	RaytracedReflection::Miss::Offset[RaytracedReflection::Ray::E_Radiance],
	//	ray,
	//	payload
	//);

	TraceRay(
		gi_BVH,
		0,
		RaytracedReflection::InstanceMask,
		0,
		2,
		0,
		ray,
		payload
	);

	go_Reflection[launchIndex] = payload.Irrad;
}

[shader("closesthit")]
void RadianceClosestHit(inout RayPayload payload, Attributes attr) {
	//uint startIndex = PrimitiveIndex() * 3;
	//const uint3 indices = { lsb_Indices[startIndex], lsb_Indices[startIndex + 1], lsb_Indices[startIndex + 2] };
	//
	//Vertex vertices[3] = {
	//	lsb_Vertices[indices[0]],
	//	lsb_Vertices[indices[1]],
	//	lsb_Vertices[indices[2]] };
	//
	//float3 normals[3] = { vertices[0].Normal, vertices[1].Normal, vertices[2].Normal };
	//float3 normal = HitAttribute(normals, attr);
	//
	//payload.Irrad = float4(normal, 1);
	payload.Irrad = float4(0, 1, 0, 1);
}

[shader("miss")]
void RadianceMiss(inout RayPayload payload) {
	payload.Irrad = float4(1, 0, 0, 1);
}	

[shader("closesthit")]
void ShadowClosestHit(inout RayPayload payload, Attributes attr) {
	payload.Irrad = 1;
	payload.IsHit = true;
}

[shader("miss")]
void ShadowMiss(inout RayPayload payload) {
	payload.Irrad = float4(0, 0, 0, 1);
	payload.IsHit = false;
}

#endif // __REFLECTIONRAY_HLSL__