#ifndef __MOTIONBLUR_HLSL__
#define __MOTIONBLUR_HLSL__

#include "Samplers.hlsli"

Texture2D gInputMap		: register(t0);
Texture2D gDepthMap		: register(t1);
Texture2D gVelocityMap	: register(t2);

cbuffer cbRootConstants : register(b0) {
	float gIntensity;
	float gLimit;
	float gDepthBias;
	float gNumSamples;
}

static const int NumSamples = 10;

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

float4 PS(VertexOut pin) : SV_TARGET{
	float2 velocity = gVelocityMap.Sample(gsamPointWrap, pin.TexC) * gIntensity;
	velocity = min(max(velocity, (float2)-gLimit), (float2)gLimit);

	float3 finalColor = gInputMap.Sample(gsamPointWrap, pin.TexC).rgb;
	float refDepth = gDepthMap.Sample(gsamDepthMap, pin.TexC).r;

	int cnt = 1;
	float2 forward = pin.TexC;
	float2 inverse = pin.TexC;
	for (int i = 0; i < gNumSamples; ++i) {
		forward += velocity;
		inverse -= velocity;

		if (refDepth < gDepthMap.Sample(gsamDepthMap, forward).r + gDepthBias) {
			finalColor += gInputMap.Sample(gsamLinearClamp, forward).rgb;
			++cnt;
		}
		if (refDepth < gDepthMap.Sample(gsamDepthMap, inverse).r + gDepthBias) {
			finalColor += gInputMap.Sample(gsamLinearClamp, inverse).rgb;
			++cnt;
		}
	}

	finalColor /= (float)cnt;

	return float4(finalColor, 1.0f);
}

#endif // __MOTIONBLUR_HLSL__