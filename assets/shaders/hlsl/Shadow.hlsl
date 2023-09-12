#ifndef __SHADOW_HLSL__
#define __SHADOW_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants>		cbPass	: register(b0);
ConstantBuffer<ObjectConstants>		cbObj	: register(b1);
ConstantBuffer<MaterialConstants>	cbMat	: register(b2);

Texture2D gi_TexMaps[NUM_TEXTURE_MAPS]	: register(t0);

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0;
	
	float4 posW = mul(float4(vin.PosL, 1), cbObj.World);
	vout.PosH = mul(posW, cbPass.ViewProj);

	float4 texC = mul(float4(vin.TexC, 0, 1), cbObj.TexTransform);
	vout.TexC = mul(texC, cbMat.MatTransform).xy;

	return vout;
}

void PS(VertexOut pin) {
	float4 albedo = cbMat.Albedo;
	if (cbMat.DiffuseSrvIndex != -1) albedo *= gi_TexMaps[cbMat.DiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(albedo.a - 0.1f);
#endif
}

#endif // __SHADOW_HLSL__