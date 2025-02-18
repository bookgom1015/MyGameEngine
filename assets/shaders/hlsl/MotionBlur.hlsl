#ifndef __MOTIONBLUR_HLSL__
#define __MOTIONBLUR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

Texture2D<SDR_FORMAT>					gi_BackBuffer	: register(t0);
Texture2D<GBuffer::DepthMapFormat>		gi_Depth		: register(t1);
Texture2D<GBuffer::VelocityMapFormat>	gi_Velocity		: register(t2);

MotionBlur_Default_RootConstants(b0)

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	const float2 pos = float2(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y);
	vout.PosH = float4(pos, 0.f, 1.f);

	return vout;
}

SDR_FORMAT PS(VertexOut pin) : SV_TARGET {
	float2 velocity = gi_Velocity.SampleLevel(gsamPointWrap, pin.TexC, 0);
	float3 colorSum = gi_BackBuffer.SampleLevel(gsamPointWrap, pin.TexC, 0).rgb;

	if (GBuffer::IsInvalidVelocity(velocity)) velocity = (float2)0.f;

	velocity *= gIntensity;
	velocity = clamp(velocity, (float2)-gLimit,(float2)gLimit);
	
	const float centerDepth = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	
	uint count = 1;
	float2 forward = pin.TexC;
	float2 inverse = pin.TexC;
	for (uint i = 0; i < gSampleCount; ++i) {
		forward += velocity;
		inverse -= velocity;
	
		if (centerDepth < gi_Depth.Sample(gsamDepthMap, forward) + gDepthBias) {
			colorSum += gi_BackBuffer.Sample(gsamLinearClamp, forward).rgb;
			++count;
		}
		if (centerDepth < gi_Depth.Sample(gsamDepthMap, inverse) + gDepthBias) {
			colorSum += gi_BackBuffer.Sample(gsamLinearClamp, inverse).rgb;
			++count;
		}
	}	
	colorSum /= count;

	return float4(colorSum, 1.f);
}

#endif // __MOTIONBLUR_HLSL__