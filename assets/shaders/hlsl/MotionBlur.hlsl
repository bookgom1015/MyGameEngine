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
	float gNumSamples;
}

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	float2 pos = float2(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y);

	// Already in homogeneous clip space.
	vout.PosH = float4(pos, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_TARGET {
	float2 velocity = gi_Velocity.Sample(gsamPointWrap, pin.TexC);
	float3 finalColor = gi_BackBuffer.Sample(gsamPointWrap, pin.TexC).rgb;

	if (velocity.x > 100) return float4(finalColor, 1);

	velocity *= gIntensity;
	velocity = min(max(velocity, (float2) - gLimit), (float2)gLimit);

	float refDepth = gi_Depth.Sample(gsamDepthMap, pin.TexC);

	int cnt = 1;
	float2 forward = pin.TexC;
	float2 inverse = pin.TexC;
	for (int i = 0; i < gNumSamples; ++i) {
		forward += velocity;
		inverse -= velocity;

		if (refDepth < gi_Depth.Sample(gsamDepthMap, forward) + gDepthBias) {
			finalColor += gi_BackBuffer.Sample(gsamLinearClamp, forward).rgb;
			++cnt;
		}
		if (refDepth < gi_Depth.Sample(gsamDepthMap, inverse) + gDepthBias) {
			finalColor += gi_BackBuffer.Sample(gsamLinearClamp, inverse).rgb;
			++cnt;
		}
	}

	finalColor /= (float)cnt;

	return float4(finalColor, 1.0f);
}

#endif // __MOTIONBLUR_HLSL__