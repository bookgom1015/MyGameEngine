// [ References ]
//  - https://lettier.github.io/3d-game-shaders-for-beginners/screen-space-reflection.html

#ifndef __SSR_VIEW_HLSL__
#define __SSR_VIEW_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

#include "Random.hlsli"

ConstantBuffer<ConstantBuffer_SSR> cb_SSR	: register(b0);

Texture2D<SDR_FORMAT>						gi_BackBuffer	: register(t0);
Texture2D<GBuffer::PositionMapFormat>		gi_Position		: register(t1);
Texture2D<GBuffer::NormalMapFormat>			gi_Normal		: register(t2);
Texture2D<DepthStencilBuffer::BufferFormat>	gi_Depth		: register(t3);
Texture2D<GBuffer::RMSMapFormat>			gi_RMS			: register(t4);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH : SV_POSITION;
	float3 PosV : POSITION;
	float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	float4 ph = mul(vout.PosH, cb_SSR.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

SSR::SSRMapFormat PS(VertexOut pin) : SV_Target {

	float depth = gi_Depth.SampleLevel(gsamDepthMap, pin.TexC, 0);
	if (GBuffer::IsInvalidDepth(depth)) return 0;

	depth = NdcDepthToViewDepth(depth, cb_SSR.Proj);

	const float3 posV = (depth / pin.PosV.z) * pin.PosV;
	if (posV.z > cb_SSR.MaxDistance) return 0;

	const float3 normalW = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);
	const float3 normalV = normalize(mul(normalW, (float3x3)cb_SSR.View));

	const float3 toEyeV = normalize(-posV);
	const float3 toLightV = reflect(-toEyeV, normalV);
	const float3 lightPosV = posV + toLightV * cb_SSR.RayLength;

	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamLinearClamp, pin.TexC).rgb;
	const float shiness = 1 - roughnessMetalicSpecular.r;

	[loop]
	for (uint i = 0; i < cb_SSR.StepCount; ++i) {
		float3 dpv = posV + toLightV * (i + 1);
		float4 ph = mul(float4(dpv, 1), cb_SSR.Proj);
		ph /= ph.w;
		if (abs(ph.y) > 1) return (float4)0;

		float2 tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;
		float d = gi_Depth.SampleLevel(gsamDepthMap, tex, 0);
		float dv = NdcDepthToViewDepth(d, cb_SSR.Proj);

		const float noise = rand_1_05(tex) * cb_SSR.NoiseIntensity;

		if (ph.z > d) {
			float3 half_r = toLightV;

			[loop]
			for (uint j = 0; j < cb_SSR.BackStepCount; ++j) {
				half_r *= 0.5;
				dpv -= half_r;
				ph = mul(float4(dpv, 1), cb_SSR.Proj);
				ph /= ph.w;

				tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;
				d = gi_Depth.SampleLevel(gsamDepthMap, tex, 0);
				dv = NdcDepthToViewDepth(d, cb_SSR.Proj);
				if (abs(dpv.z - dv) > cb_SSR.DepthThreshold) return (float4)0;

				if (ph.z < d) dpv += half_r;
			}

			ph = mul(float4(dpv, 1), cb_SSR.Proj);
			ph /= ph.w;

			tex = float2(ph.x, -ph.y) * 0.5 + (float2)0.5;

			const float3 color = gi_BackBuffer.Sample(gsamLinearClamp, tex).rgb;
			return float4(color, shiness);
		}
	}

	return (float4)0;
}

#endif // __SSR_VIEW_HLSL__