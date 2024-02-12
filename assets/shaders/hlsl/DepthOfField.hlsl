#ifndef __DEPTHOFFIELD_HLSL__
#define __DEPTHOFFIELD_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

Texture2D<ToneMapping::IntermediateMapFormat>	gi_BackBuffer	: register(t0);
Texture2D<DepthOfField::CoCMapFormat>			gi_CoC			: register(t1);

cbuffer cbRootConstants : register(b0) {
	float gBokehRadius;
	float gMaxDevTolerance;
	float gHighlightPower;
	float gSampleCount;
};

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH	: SV_POSITION;
	float2 TexC	: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	uint2 size;
	gi_BackBuffer.GetDimensions(size.x, size.y);

	const float dx = gBokehRadius / size.x;
	const float dy = gBokehRadius / size.y;

	const float centerCoC = abs(gi_CoC.Sample(gsamPointClamp, pin.TexC));
	const uint blurRadius = round(gSampleCount * centerCoC);

	const float3 centerColor = gi_BackBuffer.Sample(gsamPointClamp, pin.TexC).rgb;

	float3 poweredSum = pow(centerColor, gHighlightPower);
	float3 colorSum = gi_BackBuffer.Sample(gsamPointClamp, pin.TexC).rgb * poweredSum;

	[loop]
	for (int i = -gSampleCount; i <= gSampleCount; ++i) {
		[loop]
		for (int j = -gSampleCount; j <= gSampleCount; ++j) {
			const float radius = sqrt(i * i + j * j);
			if ((i == 0 && j == 0) || radius > blurRadius) continue;

			const float2 texc = pin.TexC + float2(i * dx, j * dy);
			const float neighborCoC = abs(gi_CoC.Sample(gsamPointClamp, texc));
			if (abs(neighborCoC - centerCoC) > gMaxDevTolerance) continue;

			const float3 color = gi_BackBuffer.Sample(gsamPointClamp, texc).rgb;
			const float3 powered = pow(color, gHighlightPower);

			colorSum += color * powered;
			poweredSum += powered;
		}
	}
	colorSum /= poweredSum;

	return float4(colorSum, 1);
}

#endif // __DEPTHOFFIELD_HLSL__