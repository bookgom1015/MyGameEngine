#ifndef __BOKEH_HLSL__
#define __BOKEH_HLSL__

#include "Samplers.hlsli"

#ifndef NUM_SAMPLES
#define NUM_SAMPLES 4
#endif

Texture2D gInputMap : register(t0);

cbuffer cbRootConstants : register(b0) {
	float gBokehRadius;
};

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float3 PosV	: POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	uint width, height;
	gInputMap.GetDimensions(width, height);
	
	float dx = gBokehRadius / (float)width;
	float dy = gBokehRadius / (float)height;
	
	float3 accum = (float3)0.0f;
	float3 total = (float3)0.0f;
	
	for (int i = -NUM_SAMPLES; i <= NUM_SAMPLES; ++i) {
		for (int j = -NUM_SAMPLES; j <= NUM_SAMPLES; ++j) {
			float radius = sqrt(i * i + j * j);
			if (radius > NUM_SAMPLES) continue;


			float2 texC = pin.TexC + float2(i * dx, j * dy);
			float3 color = gInputMap.Sample(gsamLinearClamp, texC).rgb;			
			float3 powered = pow(color, 4.0f);

			accum += color * powered;
			total += powered;
		}
	}
	
	accum /= total;
	
	return float4(accum.rgb, 1.0f);
}

#endif // __BOKEH_HLSL__