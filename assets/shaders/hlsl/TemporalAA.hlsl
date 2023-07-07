#ifndef __TEMPORALAA_HLSL__
#define __TEMPORALAA_HLSL__

#include "Samplers.hlsli"

Texture2D gInputMap		: register(t0);
Texture2D gHistoryMap	: register(t1);
Texture2D gVelocityMap	: register(t2);

cbuffer cbRootConstants : register(b0) {
	float gModulationFactor;
};

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
	uint width, height, numMips;
	gInputMap.GetDimensions(0, width, height, numMips);

	float dx = 1.0f / width;
	float dy = 1.0f / height;

	float2 velocity = gVelocityMap.Sample(gsamPointClamp, pin.TexC);
	float2 prevTexC = pin.TexC - velocity;

	float3 inputColor = gInputMap.Sample(gsamPointClamp, pin.TexC).rgb;
	float3 historyColor = gHistoryMap.Sample(gsamLinearClamp, prevTexC).rgb;

	float3 nearColor0 = gInputMap.Sample(gsamPointClamp, pin.TexC + dx).rgb;
	float3 nearColor1 = gInputMap.Sample(gsamPointClamp, pin.TexC - dx).rgb;
	float3 nearColor2 = gInputMap.Sample(gsamPointClamp, pin.TexC + dy).rgb;
	float3 nearColor3 = gInputMap.Sample(gsamPointClamp, pin.TexC - dy).rgb;

	float3 minColor = min(inputColor, min(nearColor0, min(nearColor1, min(nearColor2, nearColor3))));
	float3 maxColor = max(inputColor, max(nearColor0, max(nearColor1, max(nearColor2, nearColor3))));

	historyColor = clamp(historyColor, minColor, maxColor);

	float3 resolvedColor = inputColor * (1.0f - gModulationFactor) + historyColor * gModulationFactor;

	return float4(resolvedColor, 1.0f);
}

#endif // __TEMPORALAA_HLSL__