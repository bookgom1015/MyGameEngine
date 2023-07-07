#ifndef __BUILDINGSSR_HLSL__
#define __BUILDINGSSR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<SsrConstants> cb	: register(b0);

Texture2D<float3> gi_BackBuffer	: register(t0);
Texture2D<float3> gi_Normal		: register(t1);
Texture2D<float>  gi_Depth		: register(t2);
Texture2D<float4> gi_Specular	: register(t3);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, cb.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float rand_1_05(in float2 uv) {
	float2 noise = frac(sin(dot(uv, float2(12.9898, 78.233)* 2)) * 43758.5453);
	return abs(noise.x + noise.y) * 0.5;
}

float2 rand_2_10(in float2 uv) {
	float noiseX = frac(sin(dot(uv, float2(12.9898, 78.233) * 2)) * 43758.5453);
	float noiseY = sqrt(1 - noiseX * noiseX);
	return float2(noiseX, noiseY);
}

float2 rand_2_0004(in float2 uv) {
	float noiseX = frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
	float noiseY = frac(sin(dot(uv, float2(12.9898, 78.233) * 2)) * 43758.5453);
	return float2(noiseX, noiseY) * 0.004;
}

float4 PS(VertexOut pin) : SV_Target {
	float pz = gi_Depth.SampleLevel(gsamDepthMap, pin.TexC, 0);
	pz = NdcDepthToViewDepth(pz, cb.Proj);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	float3 pv = (pz / pin.PosV.z) * pin.PosV;
	if (pv.z > cb.MaxDistance) return (float4)0;

	float3 nw = gi_Normal.SampleLevel(gsamLinearClamp, pin.TexC, 0);
	float3 nv = normalize(mul(nw, (float3x3)cb.View));

	// Vector from point being lit to eye. 
	float3 toEyeV = normalize(-pv);

	float3 r = reflect(-toEyeV, nv) * cb.RayLength;

	[loop]
	for (uint i = 0; i < cb.NumSteps; ++i) {
		float3 dpv = pv + r * (i + 1);
		float4 ph = mul(float4(dpv, 1), cb.Proj);
		ph /= ph.w;
		if (abs(ph.y) > 1) return (float4)0;
		
		float2 tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;
		float d = gi_Depth.SampleLevel(gsamDepthMap, tex, 0);
		float dv = NdcDepthToViewDepth(d, cb.Proj);

		float noise = rand_1_05(tex) * cb.NoiseIntensity;

		if (ph.z > d) {
			float3 half_r = r;

			[loop]
			for (uint j = 0; j < cb.NumBackSteps; ++j) {
				half_r *= 0.5;
				dpv -= half_r;
				ph = mul(float4(dpv, 1), cb.Proj);
				ph /= ph.w;

				tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;
				d = gi_Depth.SampleLevel(gsamDepthMap, tex, 0);
				dv = NdcDepthToViewDepth(d, cb.Proj);
				if (abs(dpv.z - dv) > cb.DepthThreshold) return (float4)0;

				if (ph.z < d) dpv += half_r;
			}

			ph = mul(float4(dpv, 1), cb.Proj);
			ph /= ph.w;

			tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;

			float3 color = gi_BackBuffer.SampleLevel(gsamLinearClamp, tex, 0);
			return float4(color, 1);
		}
	}

	return (float4)0;
}

#endif // __BUILDINGSSR_HLSL__