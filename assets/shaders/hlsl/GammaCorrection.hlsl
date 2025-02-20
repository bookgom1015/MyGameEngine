// [ References ]
//  - https://learnopengl.com/Advanced-Lighting/Gamma-Correction

#ifndef __GAMMACORRECTION_HLSL__
#define __GAMMACORRECTION_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

GammaCorrection_Default_RootConstants(b0)

#include "CoordinatesFittedToScreen.hlsli"

Texture2D<SDR_FORMAT> gi_BackBuffer : register(t0);

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

SDR_FORMAT PS(VertexOut pin) : SV_Target{
	const float2 tex = pin.TexC;
	const float3 color = gi_BackBuffer.SampleLevel(gsamPointClamp, tex, 0).rgb;
	const float3 corrColor = pow(color, 1.f / gGamma);
	return float4(corrColor, 1.f);
}

#endif // __GAMMACORRECTION_HLSL__