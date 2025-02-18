// [ References ]
//  - https://catlikecoding.com/unity/tutorials/advanced-rendering/depth-of-field/

#ifndef __COC_HLSL__
#define __COC_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_DoF>					cb_DoF				: register(b0);

Texture2D<GBuffer::DepthMapFormat>					gi_Depth			: register(t0);

RWBuffer<DepthOfField::FocalDistanceBufferFormat>	ui_FocalDistance	: register(u0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	const float4 ph = mul(vout.PosH, cb_DoF.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

DepthOfField::CoCMapFormat PS(VertexOut pin) : SV_Target {
	float depth = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	depth = NdcDepthToViewDepth(depth, cb_DoF.Proj);

	const float focusDist = ui_FocalDistance[0];
	const float diff = depth - focusDist;
	const float coc = clamp(diff / cb_DoF.FocusRange, -1.f, 1.f);

	return coc;
}

#endif // __COC_HLSL__