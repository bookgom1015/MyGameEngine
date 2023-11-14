#ifndef __BUILDINGSSR_HLSL__
#define __BUILDINGSSR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<SsrConstants> cbSsr	: register(b0);

Texture2D<float4>							gi_BackBuffer	: register(t0);
Texture2D<GBuffer::NormalMapFormat>			gi_Normal		: register(t1);
Texture2D<DepthStencilBuffer::BufferFormat>	gi_Depth		: register(t2);

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
	float4 ph = mul(vout.PosH, cbSsr.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	float pz = gi_Depth.SampleLevel(gsamDepthMap, pin.TexC, 0);
	if (pz == DepthStencilBuffer::InvalidDepthValue) return 0;

	pz = NdcDepthToViewDepth(pz, cbSsr.Proj);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	const float3 pv = (pz / pin.PosV.z) * pin.PosV;
	if (pv.z > cbSsr.MaxDistance) return (float4)0;

	const float3 nw = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);

	const float3 nv = normalize(mul(nw, (float3x3)cbSsr.View));

	// Vector from point being lit to eye. 
	const float3 toEyeV = normalize(-pv);

	const float3 r = reflect(-toEyeV, nv) * cbSsr.RayLength;

	[loop]
	for (uint i = 0; i < cbSsr.NumSteps; ++i) {
		float3 dpv = pv + r * (i + 1);
		float4 ph = mul(float4(dpv, 1), cbSsr.Proj);
		ph /= ph.w;
		if (abs(ph.y) > 1) return (float4)0;
		
		float2 tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;
		float d = gi_Depth.SampleLevel(gsamDepthMap, tex, 0);
		float dv = NdcDepthToViewDepth(d, cbSsr.Proj);

		const float noise = rand_1_05(tex) * cbSsr.NoiseIntensity;

		if (ph.z > d) {
			float3 half_r = r;

			[loop]
			for (uint j = 0; j < cbSsr.NumBackSteps; ++j) {
				half_r *= 0.5;
				dpv -= half_r;
				ph = mul(float4(dpv, 1), cbSsr.Proj);
				ph /= ph.w;

				tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;
				d = gi_Depth.SampleLevel(gsamDepthMap, tex, 0);
				dv = NdcDepthToViewDepth(d, cbSsr.Proj);
				if (abs(dpv.z - dv) > cbSsr.DepthThreshold) return (float4)0;

				if (ph.z < d) dpv += half_r;
			}

			ph = mul(float4(dpv, 1), cbSsr.Proj);
			ph /= ph.w;

			tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;

			const float3 color = gi_BackBuffer.SampleLevel(gsamLinearClamp, tex, 0).rgb;
			return float4(color, 1);
		}
	}

	return (float4)0;
}

#endif // __BUILDINGSSR_HLSL__