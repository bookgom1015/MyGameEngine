#ifndef __REFLECTIONRAY_HLSL__
#define __REFLECTIONRAY_HLSL__

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"
#include "DxrShadingHelpers.hlsli"
#include "Samplers.hlsli"

typedef BuiltInTriangleIntersectionAttributes Attributes;

cbuffer cbRootConstants : register(b0) {
	uint	gShadowRayOffset;
	float	gReflectionRadius;
}

ConstantBuffer<PassConstants> cb_Pass : register(b1);

RaytracingAccelerationStructure							gi_BVH							: register(t0);
Texture2D<HDR_FORMAT>									gi_BackBuffer					: register(t1);
Texture2D<GBuffer::NormalMapFormat>						gi_Normal						: register(t2);
Texture2D<GBuffer::DepthMapFormat>						gi_Depth						: register(t3);
Texture2D<GBuffer::PositionMapFormat>					gi_Position						: register(t4);
TextureCube<IrradianceMap::DiffuseIrradCubeMapFormat>	gi_DiffuseIrrad					: register(t5);
Texture2D<Ssao::AOCoefficientMapFormat>					gi_AOCoeiff						: register(t6);
TextureCube<IrradianceMap::PrefilteredEnvCubeMapFormat>	gi_Prefiltered					: register(t7);
Texture2D<IrradianceMap::IntegratedBrdfMapFormat>		gi_BrdfLUT						: register(t8);
Texture2D												gi_TexMaps[NUM_TEXTURE_MAPS]	: register(t9);

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

	float3 normal = gi_Normal[launchIndex].xyz;
	float depth = gi_Depth[launchIndex];
	
	if (depth == GBuffer::InvalidNormDepthValue) {
		go_Reflection[launchIndex] = 0;
		return;
	}

	float3 origin = gi_Position[launchIndex].xyz;

	float3 fromEye = normalize(origin - cb_Pass.EyePosW);
	float3 toLight = reflect(fromEye, normal);

	RayDesc ray;
	ray.Origin = origin + 0.1 * normal;
	ray.Direction = toLight;
	ray.TMin = 0;
	ray.TMax = gReflectionRadius;

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

	float3 hitPosition = HitWorldPosition();
	float3 origin = WorldRayOrigin();

	uint2 launchIndex = DispatchRaysIndex().xy;

	const float4 albedo = lcb_Mat.Albedo;
	const float roughness = lcb_Mat.Roughness;
	const float metalic = lcb_Mat.Metalic;

	//
	// Trace shadow rays
	//
	RayDesc ray;
	ray.Origin = hitPosition + 0.1 * normal;
	ray.Direction = -cb_Pass.Lights[0].Direction;
	ray.TMin = 0;
	ray.TMax = 1000;

	TraceRay(
		gi_BVH,
		0,
		RaytracedReflection::InstanceMask,
		gShadowRayOffset,
		0,
		RaytracedReflection::Miss::Offset[RaytracedReflection::Ray::E_Shadow],
		ray,
		payload
	);

	float3 shadowFactor = 1;
	shadowFactor[0] = payload.IsHit ? 0 : 1;

	//
	// Integrate irradiances
	//
	Texture2D tex2D = gi_TexMaps[lcb_Mat.DiffuseSrvIndex];
	const float4 samp = tex2D.SampleLevel(gsamLinearClamp, texc, 0);

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * lcb_Mat.Specular, albedo.rgb, metalic);

	const float3 fromEye = normalize(hitPosition - origin);
	const float3 toLight = reflect(fromEye, normal);

	const float3 viewW = -fromEye;
	const float3 halfW = normalize(viewW + toLight);

	const float3 kS = FresnelSchlickRoughness(saturate(dot(normal, viewW)), fresnelR0, roughness);
	const float3 kD = 1 - kS;

	Material mat = { albedo, fresnelR0, shiness, metalic };

	const float3 radiance = max(ComputeBRDF(cb_Pass.Lights, mat, hitPosition, normal, viewW, shadowFactor), (float3)0);

	const float3 diffIrradSamp = gi_DiffuseIrrad.SampleLevel(gsamLinearClamp, normal, 0).xyz;
	const float3 diffuseIrradiance = diffIrradSamp * albedo.rgb;

	const float aoCoeiff = gi_AOCoeiff[launchIndex];

	const float3 reflectedW = reflect(-viewW, normal);
	const float NdotV = max(dot(normal, viewW), 0);
	const float MaxMipLevel = 5;

	const float3 prefilteredColor = gi_Prefiltered.SampleLevel(gsamLinearClamp, reflectedW, roughness * MaxMipLevel).rgb;
	const float2 envBRDF = gi_BrdfLUT.SampleLevel(gsamLinearClamp, float2(NdotV, roughness), 0);
	const float3 specRadiance = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

	const float3 ambient = (kD * diffuseIrradiance + specRadiance) * aoCoeiff;

	payload.Irrad = float4(radiance + ambient, 1);
}

[shader("miss")]
void RadianceMiss(inout RayPayload payload) {
	payload.Irrad = 0;
}	

[shader("closesthit")]
void ShadowClosestHit(inout RayPayload payload, Attributes attr) {
	payload.IsHit = true;
}

[shader("miss")]
void ShadowMiss(inout RayPayload payload) {
	payload.IsHit = false;
}

#endif // __REFLECTIONRAY_HLSL__