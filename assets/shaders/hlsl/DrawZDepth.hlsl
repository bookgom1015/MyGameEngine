#ifndef __SHADOW_HLSL__
#define __SHADOW_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Pass>		cb_Pass	: register(b0);
ConstantBuffer<ConstantBuffer_Object>	cb_Obj	: register(b1);
ConstantBuffer<ConstantBuffer_Material>	cb_Mat	: register(b2);

cbuffer cbRootConstants : register(b3) {
	uint gLightIndex;
}

Texture2D gi_TexMaps[NUM_TEXTURE_MAPS]	: register(t0);

VERTEX_IN

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0;
	
	float4 posW = mul(float4(vin.PosL, 1), cb_Obj.World);
	vout.PosH = mul(posW, cb_Pass.Lights[gLightIndex].ViewProj);

	float4 texC = mul(float4(vin.TexC, 0, 1), cb_Obj.TexTransform);
	vout.TexC = mul(texC, cb_Mat.MatTransform).xy;

	return vout;
}

void PS(VertexOut pin) {
	float4 albedo = cb_Mat.Albedo;
	if (cb_Mat.DiffuseSrvIndex != -1) albedo *= gi_TexMaps[cb_Mat.DiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(albedo.a - 0.1f);
#endif
}

#endif // __SHADOW_HLSL__