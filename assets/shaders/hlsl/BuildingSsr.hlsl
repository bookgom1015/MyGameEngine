#ifndef __BUILDINGSSR_HLSL__
#define __BUILDINGSSR_HLSL__

#include "Samplers.hlsli"
#include "SsrCommon.hlsli"

Texture2D gBackBuffer	: register(t0);
Texture2D gNormalMap	: register(t1);
Texture2D gDepthMap		: register(t2);
Texture2D gSpecularMap	: register(t3);

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

float rand_1_05(in float2 uv) {
	float2 noise = frac(sin(dot(uv, float2(12.9898, 78.233)*2.0)) * 43758.5453);
	return abs(noise.x + noise.y) * 0.5;
}

float2 rand_2_10(in float2 uv) {
	float noiseX = frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453);
	float noiseY = sqrt(1 - noiseX * noiseX);
	return float2(noiseX, noiseY);
}

float2 rand_2_0004(in float2 uv) {
	float noiseX = frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
	float noiseY = frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453);
	return float2(noiseX, noiseY) * 0.004;
}

float4 PS(VertexOut pin) : SV_Target {
	float pz = gDepthMap.Sample(gsamDepthMap, pin.TexC).r;
	pz = NdcDepthToViewDepth(pz);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	float3 pv = (pz / pin.PosV.z) * pin.PosV;
	if (pv.z > gMaxDistance) return (float4)0.0f;

	float3 nw = gNormalMap.Sample(gsamLinearClamp, pin.TexC).xyz;
	float3 nv = normalize(mul(nw, (float3x3)gView));

	// Vector from point being lit to eye. 
	float3 toEyeV = normalize(-pv);

	float3 r = reflect(-toEyeV, nv) * gRayLength;

	[loop]
	for (uint i = 0; i < gNumSteps; ++i) {
		float3 dpv = pv + r * (i + 1);
		float4 ph = mul(float4(dpv, 1.0f), gProj);
		ph /= ph.w;
		if (abs(ph.y) > 1.0f) return (float4)0.0f;

		float2 tex = float2(ph.x, -ph.y) * 0.5f + (float2)0.5f;
		float d = gDepthMap.Sample(gsamDepthMap, tex).r;
		float dv = NdcDepthToViewDepth(d);

		float noise = rand_1_05(tex) * gNoiseIntensity;

		if (ph.z > d) {
			float3 half_r = r;

			[loop]
			for (uint j = 0; j < gNumBackSteps; ++j) {
				half_r *= 0.5f;
				dpv -= half_r;
				ph = mul(float4(dpv, 1.0f), gProj);
				ph /= ph.w;

				tex = float2(ph.x, -ph.y) * 0.5f + (float2)0.5f;
				d = gDepthMap.Sample(gsamDepthMap, tex).r;
				dv = NdcDepthToViewDepth(d);
				if (abs(dpv.z - dv) > gDepthThreshold) return (float4)0.0f;

				if (ph.z < d) dpv += half_r;
			}

			ph = mul(float4(dpv, 1.0f), gProj);
			ph /= ph.w;

			tex = float2(ph.x, -ph.y) * 0.5f + (float2)0.5f;

			float3 color = gBackBuffer.Sample(gsamLinearClamp, tex).rgb;
			return float4(color, 1.0f);
		}
	}

	return (float4)0.0f;
}

#endif // __BUILDINGSSR_HLSL__