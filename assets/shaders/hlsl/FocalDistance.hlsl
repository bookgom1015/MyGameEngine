#ifndef __FOCALDISTANCE_HLSL__
#define __FOCALDISTANCE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

cbuffer cbDof : register(b0) {
	float4x4	gProj;
	float4x4	gInvProj;
	float		gFocusRange;
	float		gFocusingSpeed;
	float		gDeltaTime;
	float		gConstantPad0;
};

Texture2D<GBuffer::DepthMapFormat> gi_Depth : register(t0);

RWBuffer<float> uio_FocalDistance : register(u0);

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS() {
	VertexOut vout;

	vout.TexC = float2(0.5f, 0.5f);
	vout.PosH = float4(0.0f, 0.0f, 0.0f, 1.0f);
	//vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

void PS(VertexOut pin) {
	float depth = gi_Depth.Sample(gsamDepthMap, pin.TexC);

	depth = NdcDepthToViewDepth(depth, gProj);

	float prev = uio_FocalDistance[0];
	float diff = depth - prev;

	uio_FocalDistance[0] = prev + diff * gFocusingSpeed * gDeltaTime;
}

#endif // __FOCALDISTANCE_HLSL