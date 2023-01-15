#ifndef __DEFAULT_HLSL__
#define __DEFAULT_HLSL__

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "Common.hlsli"

struct VertexIn {
	float3 PosL		: POSITION;
	float3 NormalL	: NORMAL;
	float2 TexC		: TEXCOORD;
};

struct VertexOut {
	float4 PosH			: SV_POSITION;
	float4 ShadowPosH	: POSITION0;
	float3 PosW			: POSITION1;
	float3 NormalW		: NORMAL;
	float2 TexC			: TEXCOORD;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0.0f;

	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;

	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

	vout.PosH = mul(posW, gViewProj);

	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, gMatTransform).xy;

	// Generate projective tex-coords to project shadow map onto scene.
	vout.ShadowPosH = mul(posW, gShadowTransform);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	pin.NormalW = normalize(pin.NormalW);

	float3 toEyeW = normalize(gEyePosW - pin.PosW);

	float4 texSample = (float4)1.0f;
	if (gDiffuseSrvIndex != -1) texSample = gTextureMap[gDiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC);

	float4 ambient = gAmbientLight * gDiffuseAlbedo * texSample;

	const float shiness = 1.0f - gRoughness;
	Material mat = { gDiffuseAlbedo, gFresnelR0 , shiness };

	float3 shadowFactor = (float3)1.0f;
	shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);

	float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

	float4 litColor = ambient + directLight;
	litColor.a = gDiffuseAlbedo.a;

	return litColor;
}

#endif // __DEFAULT_HLSL__