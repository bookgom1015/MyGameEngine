#ifndef __GENERATEMIPMAP_HLSL__
#define __GENERATEMIPMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

MipmapGenerator_Default_RootConstants(b0)

Texture2D<float4> gi_Input : register(t0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

float4 PS_GenerateMipmap(VertexOut pin) : SV_Target {
	const float2 texc0 = pin.TexC;
	const float2 texc1 = pin.TexC + float2(gInvTexSize.x, 0.f);
	const float2 texc2 = pin.TexC + float2(0.f, gInvTexSize.y);
	const float2 texc3 = pin.TexC + float2(gInvTexSize.x, gInvTexSize.y);

	const float4 color0 = gi_Input.SampleLevel(gsamPointClamp, texc0, 0);
	const float4 color1 = gi_Input.SampleLevel(gsamPointClamp, texc1, 0);
	const float4 color2 = gi_Input.SampleLevel(gsamPointClamp, texc2, 0);
	const float4 color3 = gi_Input.SampleLevel(gsamPointClamp, texc3, 0);

	const float4 finalColor = (color0 + color1 + color2 + color3) * 0.25f;
	return finalColor;
}

float4 PS_JustCopy(VertexOut pin) : SV_Target{
	const float4 color = gi_Input.SampleLevel(gsamPointClamp, pin.TexC, 0);	
	return color;
}

#endif // __GENERATEMIPMAP_HLSL__