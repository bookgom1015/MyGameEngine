// [ References ]
//  - https://sugulee.wordpress.com/2021/06/21/temporal-anti-aliasingtaa-tutorial/
//  - https://xtozero.tistory.com/4

#ifndef __TEMPORALAA_HLSL__
#define __TEMPORALAA_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

Texture2D<float3> gi_Input		: register(t0);
Texture2D<float3> gi_History	: register(t1);
Texture2D<float2> gi_Velocity	: register(t2);

cbuffer cbRootConstants : register(b0) {
	float gModulationFactor;
};

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	float2 pos = float2(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y);

	// Already in homogeneous clip space.
	vout.PosH = float4(pos, 0, 1);

	return vout;
}

SDR_FORMAT PS(VertexOut pin) : SV_TARGET {
	uint2 size;
	gi_Input.GetDimensions(size.x, size.y);

	float dx = 1.0 / size.x;
	float dy = 1.0 / size.y;

	float2 velocity = gi_Velocity.Sample(gsamPointClamp, pin.TexC);
	float2 prevTexC = pin.TexC - velocity;

	float3 inputColor = gi_Input.Sample(gsamLinearClamp, pin.TexC);
	float3 historyColor = gi_History.Sample(gsamLinearClamp, prevTexC).rgb;

	float3 nearColor0 = gi_Input.Sample(gsamPointClamp, pin.TexC + dx);
	float3 nearColor1 = gi_Input.Sample(gsamPointClamp, pin.TexC - dx);
	float3 nearColor2 = gi_Input.Sample(gsamPointClamp, pin.TexC + dy);
	float3 nearColor3 = gi_Input.Sample(gsamPointClamp, pin.TexC - dy);

	float3 minColor = min(inputColor, min(nearColor0, min(nearColor1, min(nearColor2, nearColor3))));
	float3 maxColor = max(inputColor, max(nearColor0, max(nearColor1, max(nearColor2, nearColor3))));

	historyColor = clamp(historyColor, minColor, maxColor);

	float3 resolvedColor = inputColor * (1 - gModulationFactor) + historyColor * gModulationFactor;

	return float4(resolvedColor, 1);
}

#endif // __TEMPORALAA_HLSL__