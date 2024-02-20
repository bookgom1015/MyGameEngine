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

cbuffer cbRootConstants : register(b0) {
	float gIntensity;
	float gLimit;
	float gDepthBias;
	float gSampleCount;
}

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	float2 pos = float2(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y);
	vout.PosH = float4(pos, 0, 1);

	return vout;
}

SDR_FORMAT PS(VertexOut pin) : SV_TARGET {
	float2 velocity = gi_Velocity.Sample(gsamPointWrap, pin.TexC);
	float3 colorSum = gi_BackBuffer.Sample(gsamPointWrap, pin.TexC).rgb;

	if (GBuffer::IsInvalidVelocity(velocity)) velocity = 0;

	velocity *= gIntensity;
	velocity = clamp(velocity, (float2)-gLimit,(float2)gLimit);
	
	float centerDepth = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	
	int count = 1;
	float2 forward = pin.TexC;
	float2 inverse = pin.TexC;
	for (int i = 0; i < gSampleCount; ++i) {
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

	return float4(colorSum, 1);
}

#endif // __MOTIONBLUR_HLSL__