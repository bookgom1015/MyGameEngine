#ifndef __APPLYFOG_HLSL__
#define __APPLYFOG_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Pass>				cb_Pass				: register(b0);

Texture2D<GBuffer::PositionMapFormat>			gi_Position			: register(t0);
Texture3D<VolumetricLight::FrustumMapFormat>	gi_FrustumVolume	: register(t1);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

struct PixelOut {
	ToneMapping::IntermediateMapFormat	Intermediate	: SV_TARGET0;
	VolumetricLight::DebugMapFormat		Debug			: SV_TARGET1;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;
	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

PixelOut PS(VertexOut pin) {
	uint3 dims;
	gi_FrustumVolume.GetDimensions(dims.x, dims.y, dims.z);

	const GBuffer::PositionMapFormat posW = gi_Position.SampleLevel(gsamLinearClamp, pin.TexC, 0);
	
	float4 posV = mul(posW, cb_Pass.View);
	if (GBuffer::IsInvalidPosition(posW)) posV.z = dims.z - 1;
	
	const float3 texc = float3(pin.TexC, min(posV.z / dims.z, 1.f));

	const VolumetricLight::FrustumMapFormat scatteringAndTransmittance = gi_FrustumVolume.SampleLevel(gsamLinearClamp, texc, 0);
	const float3 scatteringColor = scatteringAndTransmittance.rgb;

	PixelOut pout = (PixelOut)0;
	pout.Intermediate = float4(scatteringColor, scatteringAndTransmittance.a);
	pout.Debug = float4(texc, 0.f);
	return pout;
}

#endif // __APPLYFOG_HLSL__