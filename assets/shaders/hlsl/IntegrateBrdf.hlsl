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
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	const float4 ph = mul(vout.PosH, cb_Pass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float2 IntegrateBRDF(float NdotV, float roughness) {
	roughness = max(roughness, 0.04f);

	float3 V;
	V.x = sqrt(1.f - NdotV * NdotV);
	V.y = 0.f;
	V.z = NdotV;

	float A = 0.f;
	float B = 0.f;

	float3 N = float3(0.f, 0.f, 1.f);

	[loop]
	for (uint i = 0; i < SAMPLE_COUNT; ++i)
	{
		const float2 Xi = Hammersley(i, SAMPLE_COUNT);
		const float3 H = ImportanceSampleGGX(Xi, N, roughness);
		const float3 L = normalize(2.f * dot(V, H) * H - V);

		const float NdotL = max(L.z, 0.f);
		const float NdotH = max(H.z, 0.f);
		const float VdotH = max(dot(V, H), 0.f);

		if (NdotL > 0.f)
		{
			const float G = GeometrySmith_IBL(N, V, L, roughness);
			const float G_Vis = (G * VdotH) / (NdotH * NdotV);
			const float Fc = pow(1.f - VdotH, 5.f);

			A += (1.f - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	const float inv = 1.f / (float)SAMPLE_COUNT;

	A *= inv;
	B *= inv;

	return float2(A, B);
}

float2 PS(VertexOut pin) : SV_Target{
	return IntegrateBRDF(pin.TexC.x, pin.TexC.y);
}

#endif // __INTEGRATEBRDF_HLSL__