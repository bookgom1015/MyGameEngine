#ifndef __COOKTORRANCE_HLSL__
#define __COOKTORRANCE_HLSL__

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#ifndef HLSL
#define HLSL
#endif

#ifndef COOK_TORRANCE
#define COOK_TORRANCE
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<PassConstants> cbPass	: register(b0);

Texture2D<float4>	gi_Albedo			: register(t0);
Texture2D<float3>	gi_Normal			: register(t1);
Texture2D<float>	gi_Depth			: register(t2);
Texture2D<float3>	gi_RMS				: register(t3);
Texture2D<float>	gi_Shadow			: register(t4);
Texture2D<float>	gi_AOCoefficient	: register(t5);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, cbPass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	return 0;
}

#endif // __COOKTORRANCE_HLSL__