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

cbuffer cbRootConstants : register(b0) {
	uint gShadowRayOffset;
}

ConstantBuffer<RaytracedReflectionConstantBuffer> cbRr : register(b1);

RaytracingAccelerationStructure				gi_BVH							: register(t0);
Texture2D<HDR_FORMAT>						gi_BackBuffer					: register(t1);
Texture2D<GBuffer::NormalDepthMapFormat>	gi_NormalDepth					: register(t2);
Texture2D									gi_TexMaps[NUM_TEXTURE_MAPS]	: register(t3);

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

	TraceRay(
		gi_BVH,
		0,
		RaytracedReflection::InstanceMask,
		0,
		0,
		RaytracedReflection::Miss::Offset[RaytracedReflection::Ray::E_Radiance],
		ray,
		payload
	);

	go_Reflection[launchIndex] = payload.Irrad;
}

[shader("closesthit")]
void RadianceClosestHit(inout RayPayload payload, Attributes attr) {
	uint startIndex = PrimitiveIndex() * 3;
	const uint3 indices = { lsb_Indices[startIndex], lsb_Indices[startIndex + 1], lsb_Indices[startIndex + 2] };
	
	Vertex vertices[3] = {
		lsb_Vertices[indices[0]],
		lsb_Vertices[indices[1]],
		lsb_Vertices[indices[2]] };
	
	float3 normals[3] = { vertices[0].Normal, vertices[1].Normal, vertices[2].Normal };
	float3 normal = HitAttribute(normals, attr);

	float2 texCoords[3] = { vertices[0].TexCoord, vertices[1].TexCoord, vertices[2].TexCoord };
	float2 texc = HitAttribute(texCoords, attr);

	Texture2D tex2D = gi_TexMaps[lcb_Mat.DiffuseSrvIndex];

	uint2 texSize;
	tex2D.GetDimensions(texSize.x, texSize.y);

	uint2 texIdx = texSize * texc + 0.5;

	float4 samp = tex2D.Load(uint3(texIdx, 0));
	
	payload.Irrad = samp;
}

[shader("miss")]
void RadianceMiss(inout RayPayload payload) {
	payload.Irrad = 0;
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