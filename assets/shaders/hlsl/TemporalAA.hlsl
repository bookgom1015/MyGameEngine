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

Texture2D<SDR_FORMAT>				  gi_Input		: register(t0);
Texture2D<SDR_FORMAT>				  gi_History	: register(t1);
Texture2D<GBuffer::VelocityMapFormat> gi_Velocity	: register(t2);

TemporalAA_Default_RootConstants(b0)

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	float2 pos = float2(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y);

	// Already in homogeneous clip space.
	vout.PosH = float4(pos, 0.f, 1.f);

	return vout;
}

SDR_FORMAT PS(VertexOut pin) : SV_TARGET {
	uint2 size;
	gi_Input.GetDimensions(size.x, size.y);

	const float dx = 1.f / size.x;
	const float dy = 1.f / size.y;

	const float2 velocity = gi_Velocity.Sample(gsamPointClamp, pin.TexC);
	const float2 prevTexC = pin.TexC - velocity;

	const float3 inputColor = gi_Input.SampleLevel(gsamLinearClamp, pin.TexC, 0).rgb;
	float3 historyColor = gi_History.SampleLevel(gsamLinearClamp, prevTexC, 0).rgb;

	const float3 nearColor0 = gi_Input.SampleLevel(gsamPointClamp, pin.TexC + dx, 0).rgb;
	const float3 nearColor1 = gi_Input.SampleLevel(gsamPointClamp, pin.TexC - dx, 0).rgb;
	const float3 nearColor2 = gi_Input.SampleLevel(gsamPointClamp, pin.TexC + dy, 0).rgb;
	const float3 nearColor3 = gi_Input.SampleLevel(gsamPointClamp, pin.TexC - dy, 0).rgb;

	const float3 minColor = min(inputColor, min(nearColor0, min(nearColor1, min(nearColor2, nearColor3))));
	const float3 maxColor = max(inputColor, max(nearColor0, max(nearColor1, max(nearColor2, nearColor3))));

	historyColor = clamp(historyColor, minColor, maxColor);

	const float3 resolvedColor = inputColor * (1.f - gModulationFactor) + historyColor * gModulationFactor;

	return float4(resolvedColor, 1.f);
}

#endif // __TEMPORALAA_HLSL__