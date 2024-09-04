#ifndef __SHADOWRAY_HLSL__
#define __SHADOWRAY_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"
#include "DXR_ShadingHelpers.hlsli"

struct ShadowHitInfo {
	bool IsHit;
};

ConstantBuffer<ConstantBuffer_Pass>			cb_Pass		: register(b0);

RaytracingAccelerationStructure				gi_BVH		: register(t0);
Texture2D<GBuffer::PositionMapFormat>		gi_Position	: register(t1);
Texture2D<GBuffer::NormalMapFormat>			gi_Normal	: register(t2);
Texture2D<GBuffer::DepthMapFormat>			gi_Depth	: register(t3);
RWTexture2D<DXR_Shadow::ShadowMapFormat>	gi_Shadow	: register(u0);

[shader("raygeneration")]
void ShadowRayGen() {
	uint2 launchIndex = DispatchRaysIndex().xy;

	float3 normal = gi_Normal[launchIndex].xyz;
	float d = gi_Depth[launchIndex];

	uint value = 0;

	if (d !=  GBuffer::InvalidDepthValue) {
		float3 posW = gi_Position[launchIndex].xyz;

		[loop]
		for (uint i = 0; i < cb_Pass.LightCount; ++i) {
			Light light = cb_Pass.Lights[i];

			if (light.Type == LightType::E_Point) {
				float3 direction = normalize(light.Position - posW);
				float dist = distance(posW, light.Position);

				RayDesc ray;
				ray.Origin = posW + 0.1 * direction;
				ray.Direction = direction;
				ray.TMin = 0;
				ray.TMax = dist;

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

				value = CalcShiftedShadowValueB(payload.IsHit, value, i);
			}
			else if (light.Type == LightType::E_Directional) {
				RayDesc ray;
				ray.Origin = posW + 0.1 * normal;
				ray.Direction = -light.Direction;
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

				value = CalcShiftedShadowValueB(payload.IsHit, value, i);
			}
		}
	}

	gi_Shadow[launchIndex] = value;
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