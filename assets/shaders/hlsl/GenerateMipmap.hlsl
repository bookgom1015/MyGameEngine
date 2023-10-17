#ifndef __GENERATEMIPMAP_HLSL__
#define __GENERATEMIPMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<PassConstants> cbPass : register(b0);

cbuffer cbRootConstants : register(b1) {
	float2 gInvTexSize;
	float2 gInvMipmapTexSize;
}

Texture2D<float4>	gi_Input	: register(t0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

float4 GenerateMipmapPS(VertexOut pin) : SV_Target {
	float2 texc0 = pin.TexC;
	float2 texc1 = pin.TexC + float2(gInvTexSize.x, 0);
	float2 texc2 = pin.TexC + float2(0, gInvTexSize.y);
	float2 texc3 = pin.TexC + float2(gInvTexSize.x, gInvTexSize.y);

	float4 color0 = gi_Input.SampleLevel(gsamPointClamp, texc0, 0);
	float4 color1 = gi_Input.SampleLevel(gsamPointClamp, texc1, 0);
	float4 color2 = gi_Input.SampleLevel(gsamPointClamp, texc2, 0);
	float4 color3 = gi_Input.SampleLevel(gsamPointClamp, texc3, 0);

	float4 finalColor = (color0 + color1 + color2 + color3) * 0.25;

	return finalColor;
}

float4 JustCopyPS(VertexOut pin) : SV_Target{
	float4 color = gi_Input.SampleLevel(gsamPointClamp, pin.TexC, 0);
	
	return color;
}

#endif // __GENERATEMIPMAP_HLSL__