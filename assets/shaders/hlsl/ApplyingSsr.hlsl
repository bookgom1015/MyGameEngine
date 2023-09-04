#ifndef __APPLYINGSSR_HLSL__
#define __APPLYINGSSR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

Texture2D<float4>	gi_BackBuffer	: register(t0);
Texture2D<float4>	gi_Ssr			: register(t1);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	float4 ssr = gi_Ssr.Sample(gsamLinearClamp, pin.TexC);
	float4 specular = gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC);

	float k = ssr.a;
	float specIntensity = specular.a;

	float3 applied = ((1 - k) * specular.rgb) + k * specIntensity * ssr.rgb;

	return float4(applied, 1);
}

#endif // __APPLYINGSSR_HLSL__