#ifndef __COC_HLSL__
#define __COC_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

cbuffer cbDoF : register(b0) {
	float4x4	gProj;
	float4x4	gInvProj;
	float		gFocusRange;
	float		gFocusingSpeed;
	float		gDeltaTime;
	float		gConstantPad0;
};

Texture2D<GBuffer::DepthMapFormat> gi_Depth : register(t0);

RWBuffer<DepthOfField::FocalDistanceBufferFormat> ui_FocalDistance : register(u0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

DepthOfField::CoCMapFormat PS(VertexOut pin) : SV_Target {
	float depth = gi_Depth.Sample(gsamDepthMap, pin.TexC);

	depth = NdcDepthToViewDepth(depth, gProj);

	float focusDist = ui_FocalDistance[0];

	float diff = depth - focusDist;
	float coc = diff / gFocusRange;
	coc = clamp(coc, -1, 1);

	return coc;
}

#endif // __COC_HLSL__