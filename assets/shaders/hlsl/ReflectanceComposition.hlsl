#ifndef __REFLECTANCECOMPOSITION_HLSL__
#define __REFLECTANCECOMPOSITION_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"

Texture2D<float4>	gi_Diffuse	: register(t0);
Texture2D<float4>	gi_Specular	: register(t1);

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

float4 PS(VertexOut pin) : SV_Target{
	float4 diffuse = gi_Diffuse.Sample(gsamPointClamp, pin.TexC);
	float4 specular = gi_Specular.Sample(gsamPointClamp, pin.TexC);

	return diffuse + specular;
}

#endif // __REFLECTANCECOMPOSITION_HLSL__
