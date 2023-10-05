#ifndef __CONVOLUTESPECULARIRRADIANCE_HLSL__
#define __CONVOLUTESPECULARIRRADIANCE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConvertEquirectangularToCubeConstantBuffer> cbCube : register(b0);

cbuffer cbRootConstants : register(b1) {
	uint	gFaceID;
	uint	gMipLevel;
	float	gRoughness;
}

TextureCube<float3> gi_Environment	: register(t0);

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

static const int SAMPLE_COUNT = 2048;

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout;

	vout.PosL = vin.PosL;

	float4x4 view = cbCube.View[gFaceID];
	float4 posV = mul(float4(vin.PosL, 1.0f), view);
	float4 posH = mul(posV, cbCube.Proj);

	vout.PosH = posH.xyww;

	return vout;
}

float ChetanJagsMipLevel(float3 N, float3 V, float3 H, float roughness) {
	float D = DistributionGGX(N, H, roughness);
	float NdotH = max(dot(N, H), 0);
	float HdotV = max(dot(H, V), 0);
	float pdf = D * NdotH / (4 * HdotV) + 0.0001;

	float resolution = 1024; // resolution of source cubemap (per face)
	float saTexel = 4 * PI / (6 * resolution * resolution);
	float saSample = 1 / (float(SAMPLE_COUNT) * pdf + 0.0001);

	float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

	return mipLevel;
}

float4 PS(VertexOut pin) : SV_Target{
	const float3 N = normalize(pin.PosL);
	const float3 R = N;
	const float3 V = R;

	float totalWeight = 0;
	float3 prefilteredColor = 0;

	for (uint i = 0; i < SAMPLE_COUNT; ++i) {
		const float2 Xi = Hammersley(i, SAMPLE_COUNT);
		const float3 H = ImportanceSampleGGX(Xi, N, gRoughness);
		const float3 L = normalize(2 * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0);
		if (NdotL > 0) {			
			const float mipLevel = ChetanJagsMipLevel(N, V, H, gRoughness);
			const float3 color = gi_Environment.SampleLevel(gsamLinearClamp, L, mipLevel);

			prefilteredColor += color * NdotL;
			totalWeight += NdotL;
		}
	}
	prefilteredColor = prefilteredColor / totalWeight;

	return float4(prefilteredColor, 1);
}

#endif // __CONVOLUTESPECULARIRRADIANCE_HLSL__