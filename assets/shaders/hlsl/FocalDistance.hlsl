#ifndef __FOCALDISTANCE_HLSL__
#define __FOCALDISTANCE_HLSL__

#include "Samplers.hlsli"

cbuffer cbDof : register(b0) {
	float4x4	gProj;
	float4x4	gInvProj;
	float		gFocusRange;
	float		gFocusingSpeed;
	float		gDeltaTime;
	float		gConstantPad0;
};

Texture2D gDepthMap : register(t0);

RWBuffer<float> uFocalDistance : register(u0);

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

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

void PS(VertexOut pin) {
	float depth = gDepthMap.Sample(gsamDepthMap, pin.TexC).r;

	depth = NdcDepthToViewDepth(depth);

	float prev = uFocalDistance[0];
	float diff = depth - prev;

	uFocalDistance[0] = prev + diff * gFocusingSpeed * gDeltaTime;
}

#endif // __FOCALDISTANCE_HLSL