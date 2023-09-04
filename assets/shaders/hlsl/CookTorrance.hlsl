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

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<PassConstants> cb		: register(b0);

Texture2D<float4>	gi_Color			: register(t0);
Texture2D<float4>	gi_Albedo			: register(t1);
Texture2D<float3>	gi_Normal			: register(t2);
Texture2D<float>	gi_Depth			: register(t3);
Texture2D<float4>	gi_Specular			: register(t4);
Texture2D<float>	gi_Shadow			: register(t5);
Texture2D<float>	gi_AOCoefficient	: register(t6);

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
	float4 ph = mul(vout.PosH, cb.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

//
// Trowbridge-Reitz GGX 
// 
float DistributionGGX(float3 normal, float3 halfV, float roughness) {
	float a2 = roughness * roughness;
	float NdotH = max(dot(normal, halfV), 0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return nom / denom;
}

//
// Smith's method with Schlick-GGX 
// 
// k is a remapping of ес based on whether using the geometry function 
//  for either direct lighting or IBL lighting.
float GeometryShlickGGX(float NdotV, float k) {
	float nom = NdotV;
	float denom = NdotV * (1 - k) + k;

	return nom / denom;
}

float GeometrySmith(float3 normal, float3 view, float3 light, float k) {
	float NdotV = max(dot(normal, view), 0);
	float NdotL = max(dot(normal, light), 0);

	float ggx1 = GeometryShlickGGX(NdotV, k);
	float ggx2 = GeometryShlickGGX(NdotL, k);

	return ggx1 * ggx2;
}

//
// the Fresnel Schlick approximation
//
float3 FresnelShlick(float cos, float3 F0) {
	return F0 + (1 - F0) * pow(1 - cos, 5);
}

float4 PS(VertexOut pin) : SV_Target {
	return (float4)1;
}

#endif // __COOKTORRANCE_HLSL__