#ifndef __GBUFFER_HLSL__
#define __GBUFFER_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants>		cbPass	: register(b0);
ConstantBuffer<ObjectConstants>		cbObj	: register(b1);
ConstantBuffer<MaterialConstants>	cbMat	: register(b2);

Texture2D gi_TexMaps[NUM_TEXTURE_MAPS]		: register(t0);

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH			: SV_POSITION;
	float4 NonJitPosH	: POSITION0;
	float4 PrevPosH		: POSITION1;
	float3 PosW			: POSITION2;
	float3 PrevPosW		: POSITION3;
	float3 NormalW		: NORMAL;
	float3 PrevNormalW	: PREVNORMAL;
	float2 TexC			: TEXCOORD;
};

struct PixelOut {
	float4 Color					: SV_TARGET0;
	float4 NormalDepth				: SV_TARGET1;
	float4 RoughnessMetalicSpecular	: SV_TARGET2;
	float4 Velocity					: SV_TARGET3;
	float4 ReprojNormalDepth		: SV_TARGET4;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0;

	float4 posW = mul(float4(vin.PosL, 1), cbObj.World);
	vout.PosW = posW.xyz;

	vout.NormalW = mul(vin.NormalL, (float3x3)cbObj.World);
	vout.PrevNormalW = mul(vin.NormalL, (float3x3)cbObj.PrevWorld);

	vout.NonJitPosH = mul(posW, cbPass.ViewProj);
	vout.PosH = vout.NonJitPosH + float4(cbPass.JitteredOffset * vout.NonJitPosH.w, 0, 0);

	float4 prevPosW = mul(float4(vin.PosL, 1), cbObj.PrevWorld);
	vout.PrevPosW = prevPosW.xyz;
	vout.PrevPosH = mul(prevPosW, cbPass.PrevViewProj);

	float4 texC = mul(float4(vin.TexC, 0, 1), cbObj.TexTransform);
	vout.TexC = mul(texC, cbMat.MatTransform).xy;
	
	return vout;
}

PixelOut PS(VertexOut pin) {
	pin.NormalW = normalize(pin.NormalW);
	pin.PrevNormalW = normalize(pin.PrevNormalW);

	float4 albedo = cbMat.Albedo;
	if (cbMat.DiffuseSrvIndex != -1) albedo *= gi_TexMaps[cbMat.DiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	
	pin.NonJitPosH /= pin.NonJitPosH.w;
	pin.PrevPosH /= pin.PrevPosH.w;
	const float2 velocity = CalcVelocity(pin.NonJitPosH, pin.PrevPosH);

	PixelOut pout = (PixelOut)0;
	pout.Color = albedo;
	pout.NormalDepth = float4(pin.NormalW, pin.NonJitPosH.z);
	pout.RoughnessMetalicSpecular = float4(cbMat.Roughness, cbMat.Metalic, cbMat.Specular, 0);
	pout.Velocity = float4(velocity, 0, 1);
	pout.ReprojNormalDepth = float4(pin.PrevNormalW, pin.PrevPosH.z);
	return pout;
}

#endif // __GBUFFER_HLSL__