#ifndef __INTEGRATEBRDF_HLSL__
#define __INTEGRATEBRDF_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "BRDF.hlsli"

ConstantBuffer<ConstantBuffer_Pass> cb_Pass : register(b0);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

static const uint SAMPLE_COUNT = 1024;

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	float4 ph = mul(vout.PosH, cb_Pass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float2 IntegrateBRDF(float NdotV, float roughness) {
	float3 V;
	V.x = sqrt(1 - NdotV * NdotV);
	V.y = 0;
	V.z = NdotV;

	float A = 0;
	float B = 0;

	float3 N = float3(0, 0, 1);

	for (uint i = 0; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, N, roughness);
		float3 L = normalize(2 * dot(V, H) * H - V);

		float NdotL = max(L.z, 0);
		float NdotH = max(H.z, 0);
		float VdotH = max(dot(V, H), 0);

		if (NdotL > 0)
		{
			float G = GeometrySmith_IBL(N, V, L, roughness);
			float G_Vis = (G * VdotH) / (NdotH * NdotV);
			float Fc = pow(1 - VdotH, 5);

			A += (1 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	const float inv = 1 / float(SAMPLE_COUNT);

	A *= inv;
	B *= inv;

	return float2(A, B);
}

float2 PS(VertexOut pin) : SV_Target{
	float2 integrated = IntegrateBRDF(pin.TexC.x, pin.TexC.y);
	return integrated;
}

#endif // __INTEGRATEBRDF_HLSL__