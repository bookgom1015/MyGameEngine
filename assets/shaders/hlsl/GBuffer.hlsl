#ifndef __GBUFFER_HLSL__
#define __GBUFFER_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

#include "ValuePackaging.hlsli"
#include "ValueTypeConversion.hlsli"

ConstantBuffer<ConstantBuffer_Pass>		cb_Pass	: register(b0);
ConstantBuffer<ConstantBuffer_Object>	cb_Obj	: register(b1);
ConstantBuffer<ConstantBuffer_Material>	cb_Mat	: register(b2);

cbuffer cbRootConstant : register(b3) {
	float gMaxDistance;
	float gMinDistance;
}

Texture2D gi_TexMaps[NUM_TEXTURE_MAPS]			: register(t0);

static const float4x4 ThresholdMatrix = {
	1.0  / 17.0, 9.0  / 17.0, 3.0  / 17.0, 11.0 / 17.0,
	13.0 / 17.0, 5.0  / 17.0, 15.0 / 17.0, 7.0  / 17.0,
	4.0  / 17.0, 12.0 / 17.0, 2.0  / 17.0, 10.0 / 17.0,
	16.0 / 17.0, 8.0  / 17.0, 14.0 / 17.0, 6.0  / 17.0
};

VERTEX_IN

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
	GBuffer::AlbedoMapFormat			Color						: SV_TARGET0;
	GBuffer::NormalMapFormat			Normal						: SV_TARGET1;
	GBuffer::NormalDepthMapFormat		NormalDepth					: SV_TARGET2;
	GBuffer::RMSMapFormat				RoughnessMetalicSpecular	: SV_TARGET3;
	GBuffer::VelocityMapFormat			Velocity					: SV_TARGET4;
	GBuffer::ReprojNormalDepthMapFormat	ReprojNormalDepth			: SV_TARGET5;
	GBuffer::PositionMapFormat			Position					: SV_TARGET6;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0;

	float4 posW = mul(float4(vin.PosL, 1), cb_Obj.World);
	vout.PosW = posW.xyz;

	vout.NormalW = mul(vin.NormalL, (float3x3)cb_Obj.World);
	vout.PrevNormalW = mul(vin.NormalL, (float3x3)cb_Obj.PrevWorld);

	vout.NonJitPosH = mul(posW, cb_Pass.ViewProj);
	vout.PosH = vout.NonJitPosH + float4(cb_Pass.JitteredOffset * vout.NonJitPosH.w, 0, 0);

	float4 prevPosW = mul(float4(vin.PosL, 1), cb_Obj.PrevWorld);
	vout.PrevPosW = prevPosW.xyz;
	vout.PrevPosH = mul(prevPosW, cb_Pass.PrevViewProj);

	float4 texC = mul(float4(vin.TexC, 0, 1), cb_Obj.TexTransform);
	vout.TexC = mul(texC, cb_Mat.MatTransform).xy;
	
	return vout;
}

PixelOut PS(VertexOut pin) {
	const uint2 screenPos = (pin.NonJitPosH.xy * 0.5 + 0.5) * uint2(1280, 720);
	const float threshold = ThresholdMatrix[screenPos.x % 4][screenPos.y % 4];

	float4 posV = mul(pin.NonJitPosH, cb_Pass.InvProj);
	posV /= posV.w;

	float dist = posV.z - gMinDistance;
	dist = dist / (gMaxDistance - gMinDistance);

	clip(dist - threshold);

	float4 albedo = cb_Mat.Albedo;
	if (cb_Mat.DiffuseSrvIndex != -1) {
		float4 corrected = sRGBToLinear(gi_TexMaps[cb_Mat.DiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC));
		albedo *= corrected;
	}
	
	pin.NonJitPosH /= pin.NonJitPosH.w;
	pin.PrevPosH /= pin.PrevPosH.w;
	const float2 velocity = CalcVelocity(pin.NonJitPosH, pin.PrevPosH);

	float3 normal = normalize(pin.NormalW);

	PixelOut pout = (PixelOut)0;
	pout.Color = albedo;
	pout.Normal = float4(normal, 0);
	pout.NormalDepth = EncodeNormalDepth(normal, pin.NonJitPosH.z);
	pout.RoughnessMetalicSpecular = float4(cb_Mat.Roughness, cb_Mat.Metalic, cb_Mat.Specular, 0);
	pout.Velocity = velocity;
	pout.ReprojNormalDepth = EncodeNormalDepth(pin.PrevNormalW, pin.PrevPosH.z);
	pout.Position = float4(pin.PosW, 1);
	return pout;
}

#endif // __GBUFFER_HLSL__