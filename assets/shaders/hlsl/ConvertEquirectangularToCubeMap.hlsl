// [ References ]
//  - https://learnopengl.com/PBR/IBL/Diffuse-irradiance
//  - https://learnopengl.com/PBR/IBL/Specular-IBL

#ifndef __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__
#define __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

#include "Equirectangular.hlsli"

ConstantBuffer<ConstantBuffer_Irradiance>	cb_Irrad			: register(b0);

Texture2D<HDR_FORMAT>						gi_Equirectangular	: register(t0);

#include "HardCodedCubeVertices.hlsli"

struct VertexOut {
	float3 PosL		: POSITION;
};

struct GeoOut {
	float4	PosH		: SV_POSITION;
	float3	PosL		: POSITION;
	uint	ArrayIndex	: SV_RenderTargetArrayIndex;
};

static const float2 InvATan = float2(0.1591f, 0.3183f);

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.PosL = gVertices[vid];

	return vout;
}

[maxvertexcount(18)]
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triStream) {
	GeoOut gout = (GeoOut)0;

	[unroll]
	for (uint face = 0; face < 6; ++face) {
		gout.ArrayIndex = face;
		const float4x4 view = cb_Irrad.View[face];
		[unroll]
		for (uint i = 0; i < 3; ++i) {
			const float3 posL = gin[i].PosL;
			const float4 posV = mul(float4(posL, 1), view);
			const float4 posH = mul(posV, cb_Irrad.Proj);

			gout.PosL = posL;
			gout.PosH = posH.xyww;

			triStream.Append(gout);
		}
		triStream.RestartStrip();
	}
}

HDR_FORMAT PS(GeoOut pin) : SV_Target{
	const float2 texc = Equirectangular::SampleSphericalMap(normalize(pin.PosL));
	const float3 samp = gi_Equirectangular.Sample(gsamLinearClamp, texc).rgb;
	return float4(samp, 1.f);
}

#endif // __CONVERTEQUIRECTANGULARTOCUBEMAP_HLSL__