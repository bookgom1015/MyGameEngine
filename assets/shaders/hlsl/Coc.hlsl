#ifndef __COC_HLSL__
#define __COC_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
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

RWBuffer<float> ui_FocalDistance : register(u0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

DepthOfField::CocMapFormat PS(VertexOut pin) : SV_Target {
	float depth = gi_Depth.Sample(gsamDepthMap, pin.TexC);

	depth = NdcDepthToViewDepth(depth);

	float focusDist = ui_FocalDistance[0];

	float diff = depth - focusDist;
	float coc = diff / gFocusRange;
	coc = clamp(coc, -1, 1);

	return coc;
}

#endif // __COC_HLSL__