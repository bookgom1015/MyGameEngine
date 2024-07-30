#ifndef __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__
#define __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Irradiance> cb_Irrad	: register(b0);

Texture2D<HDR_FORMAT> gi_Equirectangular : register(t0);

#include "HardCodedCubeVertices.hlsli"

struct VertexOut {
	float3 PosL		: POSITION;
};

struct GeoOut {
	float4	PosH		: SV_POSITION;
	float3	PosL		: POSITION;
	uint	ArrayIndex	: SV_RenderTargetArrayIndex;
};

static const float2 InvATan = float2(0.1591, 0.3183);

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.PosL = gVertices[vid];

	return vout;
}

[maxvertexcount(18)]
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triStream) {
	GeoOut gout = (GeoOut)0;

	[unroll]
	for (int face = 0; face < 6; ++face) {
		gout.ArrayIndex = face;
		float4x4 view = cb_Irrad.View[face];
		[unroll]
		for (int i = 0; i < 3; ++i) {
			float3 posL = gin[i].PosL;
			float4 posV = mul(float4(posL, 1), view);
			float4 posH = mul(posV, cb_Irrad.Proj);

			gout.PosL = posL;
			gout.PosH = posH.xyww;

			triStream.Append(gout);
		}
		triStream.RestartStrip();
	}
}

float2 SampleSphericalMap(float3 view) {
	float2 texc = float2(atan2(view.z, view.x), asin(view.y));
	texc *= InvATan;
	texc += 0.5;
	texc.y = 1 - texc.y;
	return texc;
}

HDR_FORMAT PS(GeoOut pin) : SV_Target{
	float2 texc = SampleSphericalMap(normalize(pin.PosL));
	float3 samp = gi_Equirectangular.Sample(gsamLinearClamp, texc).rgb;
	return float4(samp, 1);
}

#endif // __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__