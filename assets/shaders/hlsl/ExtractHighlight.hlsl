// [ References ]
//  - https://nellfamily.tistory.com/50
//  - https://sksnxjs.tistory.com/entry/Graphics-Bloom-shader-effect
//   -- https://en.wikipedia.org/wiki/Bloom_(shader_effect)
//   -- https://jmhongus2019.wixsite.com/mysite
//   -- https://learnopengl.com/Advanced-Lighting/Bloom
//   -- https://learnopengl.com/Advanced-Lighting/Bloom

#ifndef __EXTRACTHIGHLIGHT_HLSL__
#define __EXTRACTHIGHLIGHT_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

Texture2D<ToneMapping::IntermediateMapFormat> gi_BackBuffer : register(t0);

Bloom_ExtractHighlight_RootConstants(b0)

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	const float3 color = gi_BackBuffer.Sample(gsamLinearWrap, pin.TexC).rgb;
	const float brightness = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));

	if (brightness > gThreshold) return float4(color, 1.f);
	else return 0;
}

#endif // __EXTRACTHIGHLIGHT_HLSL__