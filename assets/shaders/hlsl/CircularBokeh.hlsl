// [ References ]
//  - https://catlikecoding.com/unity/tutorials/advanced-rendering/depth-of-field/

#ifndef __CIRCULARBOKEH_HLSL__
#define __CIRCULARBOKEH_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

Texture2D<ToneMapping::IntermediateMapFormat>	gi_BackBuffer	: register(t0);
Texture2D<DepthOfField::CoCMapFormat>			gi_CoC			: register(t1);

DepthOfField_ApplyDoF_RootConstants(b0)

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	uint2 size;
	gi_BackBuffer.GetDimensions(size.x, size.y);

	const float dx = gBokehRadius / size.x;
	const float dy = gBokehRadius / size.y;

	const float centerCoC = abs(gi_CoC.SampleLevel(gsamPointClamp, pin.TexC, 0));
	const uint blurRadius = round(gSampleCount * centerCoC);

	const float3 centerColor = gi_BackBuffer.SampleLevel(gsamPointClamp, pin.TexC, 0).rgb;

	float3 poweredSum = pow(centerColor, gHighlightPower);
	float3 colorSum = gi_BackBuffer.SampleLevel(gsamPointClamp, pin.TexC, 0).rgb * poweredSum;

	[loop]
	for (int i = -gSampleCount; i <= gSampleCount; ++i) {
		[loop]
		for (int j = -gSampleCount; j <= gSampleCount; ++j) {
			const float radius = sqrt(i * i + j * j);
			if ((i == 0 && j == 0) || radius > blurRadius) continue;

			const float2 texc = pin.TexC + float2(i * dx, j * dy);
			const float neighborCoC = abs(gi_CoC.SampleLevel(gsamPointClamp, texc, 0));
			if (abs(neighborCoC - centerCoC) > gMaxDevTolerance) continue;

			const float3 color = gi_BackBuffer.SampleLevel(gsamPointClamp, texc, 0).rgb;
			const float3 powered = pow(color, gHighlightPower);

			colorSum += color * powered;
			poweredSum += powered;
		}
	}
	colorSum /= poweredSum;

	return float4(colorSum, 1.f);
}

#endif // __CIRCULARBOKEH_HLSL__