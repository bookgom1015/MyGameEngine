#ifndef __CONVOLUTEDIFFUSEIRRADIANCE_HLSL__
#define __CONVOLUTEDIFFUSEIRRADIANCE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Irradiance> cb_Irrad	: register(b0);

cbuffer cbRootConstants : register(b1) {
	uint	gFaceID;
	float	gSampleDelta;
}

TextureCube<HDR_FORMAT> gi_Cube : register(t0);

VERTEX_IN

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) {
	VertexOut vout;

	vout.PosL = vin.PosL;
	
	float4x4 view = cb_Irrad.View[gFaceID];
	float4 posV = mul(float4(vin.PosL, 1), view);
	float4 posH = mul(posV, cb_Irrad.Proj);
	
	vout.PosH = posH.xyww;

	return vout;
}

float3 ConvoluteIrradiance(float3 pos) {
	// The world vector acts as the normal of a tangent surface
	// from the origin, aligned to WorldPos. Given this normal, calculate all
	// incoming radiance of the environment. The result of this radiance
	// is the radiance of light coming from -Normal direction, which is what
	// we use in the PBR shader to sample irradiance.
	const float3 N = normalize(pos);

	float3 irradiance = 0;

	float3 up = float3(0, 1, 0);
	const float3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));

	uint numSamples = 0;
	float sampDelta = 0.0125;
	for (float phi = 0; phi < 2 * PI; phi += sampDelta) {
		for (float theta = 0; theta < 0.5 * PI; theta += sampDelta) {
			const float cosTheta = cos(theta);
			const float sinTheta = sin(theta);

			// Spherical to cartesian (in tangent space)
			const float3 tangentSample = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
			// tangent space to world space
			const float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

			irradiance += gi_Cube.Sample(gsamLinearClamp, sampleVec).rgb * cosTheta * sinTheta;
			++numSamples;
		}
	}
	irradiance = PI * irradiance * (1 / float(numSamples));

	return irradiance;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target{
	float3 irradiance = ConvoluteIrradiance(pin.PosL);
	
	return float4(irradiance, 1);
}

#endif // __CONVOLUTEDIFFUSEIRRADIANCE_HLSL__