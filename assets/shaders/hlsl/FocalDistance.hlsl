// [ References ]
//  - https://dev.epicgames.com/documentation/en-us/unreal-engine/cinematic-depth-of-field-method?application_version=4.27

#ifndef __FOCALDISTANCE_HLSL__
#define __FOCALDISTANCE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_DoF>					cb_DoF				: register(b0);

Texture2D<GBuffer::DepthMapFormat>					gi_Depth			: register(t0);

RWBuffer<DepthOfField::FocalDistanceBufferFormat>	uio_FocalDistance	: register(u0);

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS() {
	VertexOut vout;

	vout.TexC = float2(0.5f, 0.5f);
	vout.PosH = float4(0.f, 0.f, 0.f, 1.f);

	const float4 ph = mul(vout.PosH, cb_DoF.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

void PS(VertexOut pin) {
	float depth = gi_Depth.SampleLevel(gsamDepthMap, pin.TexC, 0);
	depth = NdcDepthToViewDepth(depth, cb_DoF.Proj);

	const float prev = uio_FocalDistance[0];
	const float diff = depth - prev;

	uio_FocalDistance[0] = prev + diff * cb_DoF.FocusingSpeed * cb_DoF.DeltaTime;
}

#endif // __FOCALDISTANCE_HLSL