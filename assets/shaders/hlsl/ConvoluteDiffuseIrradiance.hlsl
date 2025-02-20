// [ References ]
//  - https://learnopengl.com/PBR/IBL/Diffuse-irradiance

#ifndef __CONVOLUTEDIFFUSEIRRADIANCE_HLSL__
#define __CONVOLUTEDIFFUSEIRRADIANCE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "Samplers.hlsli"

#include "BRDF.hlsli"

ConstantBuffer<ConstantBuffer_Irradiance>	cb_Irrad	: register(b0);

TextureCube<HDR_FORMAT>						gi_Cube		: register(t0);

#include "HardCodedCubeVertices.hlsli"

struct VertexOut {
	float3 PosL			: POSITION;
};

struct GeoOut {
	float4 PosH			: SV_POSITION;
	float3 PosL			: POSITION;
	uint   ArrayIndex	: SV_RenderTargetArrayIndex;
};

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

float3 ConvoluteIrradiance(float3 pos) {
	// The world vector acts as the normal of a tangent surface
	// from the origin, aligned to WorldPos. Given this normal, calculate all
	// incoming radiance of the environment. The result of this radiance
	// is the radiance of light coming from -Normal direction, which is what
	// we use in the PBR shader to sample irradiance.
	const float3 N = normalize(pos);

	float3 irradiance = (float3)0.f;

	float3 up = float3(0.f, 1.f, 0.f);
	const float3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));

	uint numSamples = 0;
	float sampDelta = 0.0125f;
	for (float phi = 0.f, phi_end = 2.f * PI; phi < phi_end; phi += sampDelta) {
		for (float theta = 0.f, theta_end = 0.5f * PI; theta < theta_end; theta += sampDelta) {
			const float cosTheta = cos(theta);
			const float sinTheta = sin(theta);

			// Spherical to cartesian (in tangent space)
			const float3 tangentSample = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
			// tangent space to world space
			const float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

			float4 gammaDecoded = gi_Cube.Sample(gsamLinearClamp, sampleVec);
			irradiance += gammaDecoded.rgb * cosTheta * sinTheta;
			++numSamples;
		}
	}
	irradiance = PI * irradiance * (1.f / (float)numSamples);

	return irradiance;
}

HDR_FORMAT PS(GeoOut pin) : SV_Target{
	const float3 irradiance = ConvoluteIrradiance(pin.PosL);	
	return float4(irradiance, 1.f);
}

#endif // __CONVOLUTEDIFFUSEIRRADIANCE_HLSL__