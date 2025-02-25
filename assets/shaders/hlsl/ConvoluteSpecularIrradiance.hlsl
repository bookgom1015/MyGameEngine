// [ References ]
//  - https://learnopengl.com/PBR/IBL/Specular-IBL

#ifndef __CONVOLUTESPECULARIRRADIANCE_HLSL__
#define __CONVOLUTESPECULARIRRADIANCE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

#include "BRDF.hlsli"

ConstantBuffer<ConstantBuffer_Irradiance>	cb_Irrad		: register(b0);

IrradianceMap_ConvoluteSpecularIrradiance_RootConstants(b1)

TextureCube<HDR_FORMAT>						gi_Environment	: register(t0);

#include "HardCodedCubeVertices.hlsli"

struct VertexOut {
	float3 PosL		: POSITION;
};

struct GeoOut {
	float4	PosH		: SV_POSITION;
	float3	PosL		: POSITION;
	uint	ArrayIndex	: SV_RenderTargetArrayIndex;
};

static const int SAMPLE_COUNT = 2048;

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.PosL = gVertices[vid];

	return vout;
}

[maxvertexcount(18)]
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triStream) {
	GeoOut gout = (GeoOut)0;

	[unroll]
	for (uint face = 0; face < 6; ++face) {
		const float4x4 view = cb_Irrad.View[face];
		[unroll]
		for (uint i = 0; i < 3; ++i) {
			const float3 posL = gin[i].PosL;
			const float4 posV = mul(float4(posL, 1.f), view);
			const float4 posH = mul(posV, cb_Irrad.Proj);

			gout.PosL = posL;
			gout.PosH = posH.xyww;
			gout.ArrayIndex = face;

			triStream.Append(gout);
		}
		triStream.RestartStrip();
	}
}

float ChetanJagsMipLevel(in float3 N, in float3 V, in float3 H, in float roughness) {
	const float D = DistributionGGX(N, H, roughness);
	const float NdotH = max(dot(N, H), 0.f);
	const float HdotV = max(dot(H, V), 0.f);
	const float pdf = D * NdotH / (4.f * HdotV) + 0.0001f;

	const float resolution2 = gResolution * gResolution;
	const float saTexel = 4.f * PI / (6.f * resolution2);
	const float saSample = 1.f / ((float)SAMPLE_COUNT * pdf + 0.0001f);

	const float mipLevel = roughness == 0.f ? 0.f : 0.5f * log2(saSample / saTexel);
	return mipLevel;
}

HDR_FORMAT PS(GeoOut pin) : SV_Target{
	const float3 N = normalize(pin.PosL);
	const float3 R = N;
	const float3 V = R;

	float totalWeight = 0.f;
	float3 prefilteredColor = 0.f;

	for (uint i = 0; i < SAMPLE_COUNT; ++i) {
		const float2 Xi = Hammersley(i, SAMPLE_COUNT);
		const float3 H = ImportanceSampleGGX(Xi, N, gRoughness);
		const float3 L = normalize(2.f * dot(V, H) * H - V);

		const float NdotL = max(dot(N, L), 0.f);
		if (NdotL > 0.f) {			
			const float mipLevel = ChetanJagsMipLevel(N, V, H, gRoughness);

			prefilteredColor += gi_Environment.SampleLevel(gsamLinearClamp, L, mipLevel).rgb * NdotL;
			totalWeight += NdotL;
		}
	}
	prefilteredColor = prefilteredColor / totalWeight;

	return float4(prefilteredColor, 1.f);
}

#endif // __CONVOLUTESPECULARIRRADIANCE_HLSL__