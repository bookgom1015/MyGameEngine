// [ References ]
//  - https://learnopengl.com/Advanced-Lighting/Gamma-Correction

#ifndef __GAMMACORRECTION_HLSL__
#define __GAMMACORRECTION_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

cbuffer gRootConstants : register(b0) {
	float gGamma;
}

#include "CoordinatesFittedToScreen.hlsli"

Texture2D<SDR_FORMAT> gi_BackBuffer : register(t0);

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID, uint instanceID : SV_InstanceID) {
	VertexOut vout = (VertexOut)0.0f;

	vout.TexC = gTexCoords[vid];

	float2 pos = float2(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y);
	vout.PosH = float4(pos, 0, 1);

	return vout;
}

SDR_FORMAT PS(VertexOut pin) : SV_Target{
	float2 tex = pin.TexC;

	float3 color = gi_BackBuffer.SampleLevel(gsamPointClamp, tex, 0).rgb;

	float3 corrColor = pow(color, 1 / gGamma);

	return float4(corrColor, 1);
}

#endif // __GAMMACORRECTION_HLSL__