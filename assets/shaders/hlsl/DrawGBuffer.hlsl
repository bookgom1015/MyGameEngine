#ifndef __DRAWGBUFFER_HLSL__
#define __DRAWGBUFFER_HLSL__

#include "Common.hlsli"

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
	float2 TexC			: TEXCOORD;
};

struct PixelOut {
	float4 Color	: SV_TARGET0;
	float4 Albedo	: SV_TARGET1;
	float4 Normal	: SV_TARGET2;
	float4 Specular	: SV_TARGET3;
	float4 Velocity	: SV_TARGET4;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0.0f;

	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;

	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

	vout.NonJitPosH = mul(posW, gViewProj);
	vout.PosH = vout.NonJitPosH + float4(gJitteredOffset * vout.NonJitPosH.w, 0.0f, 0.0f);

	float4 prevPosW = mul(float4(vin.PosL, 1.0f), gPrevWorld);
	vout.PrevPosW = prevPosW.xyz;

	vout.PrevPosH = mul(prevPosW, gPrevViewProj);

	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, gMatTransform).xy;
	
	return vout;
}

PixelOut PS(VertexOut pin) {
	pin.NonJitPosH /= pin.NonJitPosH.w;
	pin.PrevPosH /= pin.PrevPosH.w;
	pin.NormalW = normalize(pin.NormalW);

	float3 toEyeW = normalize(gEyePosW - pin.PosW);

	float4 texSample = (float4)1.0f;
	if (gDiffuseSrvIndex != -1) texSample = gTextureMap[gDiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	
	float2 velocity = CalcVelocity(pin.NonJitPosH, pin.PrevPosH);

	PixelOut pout = (PixelOut)0.0f;
	pout.Color = texSample;
	pout.Albedo = gDiffuseAlbedo;
	pout.Normal = float4(pin.NormalW, 0.0f);
	pout.Specular = float4(gFresnelR0, gRoughness);
	pout.Velocity = float4(velocity, 0.0f, 1.0f);
	return pout;
}

#endif // __DRAWGBUFFER_HLSL__