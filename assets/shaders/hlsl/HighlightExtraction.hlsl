// [ References ]
//  - https://nellfamily.tistory.com/50
//  - https://sksnxjs.tistory.com/entry/Graphics-Bloom-shader-effect
//   -- https://en.wikipedia.org/wiki/Bloom_(shader_effect)
//   -- https://jmhongus2019.wixsite.com/mysite
//   -- https://learnopengl.com/Advanced-Lighting/Bloom
//   -- https://learnopengl.com/Advanced-Lighting/Bloom

#ifndef __HIGHLIGHTEXTRACTION_HLSL__
#define __HIGHLIGHTEXTRACTION_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"

Texture2D<ToneMapping::IntermediateMapFormat> gi_BackBuffer : register(t0);

Bloom_Default_RootConstants(b0)

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	float3 color = gi_BackBuffer.Sample(gsamLinearWrap, pin.TexC).rgb;
	float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));

	if (brightness > gThreshold) return float4(color, 1);
	else return 0;
}

#endif // __HIGHLIGHTEXTRACTION_HLSL__