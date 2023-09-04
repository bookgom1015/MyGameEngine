#ifndef __BLINNPHONG_HLSL__
#define __BLINNPHONG_HLSL__

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

float4 PS(VertexOut pin) : SV_Target{
	// Get viewspace normal and z-coord of this pixel.  
	float pz = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	pz = NdcDepthToViewDepth(pz, cb.Proj);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	float3 posV = (pz / pin.PosV.z) * pin.PosV;
	float4 posW = mul(float4(posV, 1), cb.InvView);
	
	float3 normalW = normalize(gi_Normal.Sample(gsamAnisotropicWrap, pin.TexC));	
	float3 toEyeW = normalize(cb.EyePosW - posW.xyz);
	
	float4 colorSample = gi_Color.Sample(gsamAnisotropicWrap, pin.TexC);
	float4 albedoSample = gi_Albedo.Sample(gsamAnisotropicWrap, pin.TexC);
	float4 diffuseAlbedo = colorSample * albedoSample;

	float4 ssaoPosH = mul(posW, cb.ViewProjTex);
	ssaoPosH /= ssaoPosH.w;
	float ambientAccess = gi_AOCoefficient.Sample(gsamAnisotropicWrap, ssaoPosH.xy, 0);

	float4 ambient = ambientAccess * cb.AmbientLight * diffuseAlbedo;
	
	float4 specular = gi_Specular.Sample(gsamAnisotropicWrap, pin.TexC);
	const float shiness = 1.0f - specular.a;
	Material mat = { albedoSample, specular.rgb, shiness };
	
	float3 shadowFactor = 0;
	float4 shadowPosH = mul(posW, cb.ShadowTransform);
	shadowFactor[0] = CalcShadowFactor(gi_Shadow, gsamShadow, shadowPosH);
	
	float4 directLight = ComputeLighting(cb.Lights, mat, posW, normalW, toEyeW, shadowFactor);
	
	float4 litColor = ambient + directLight;
	litColor.a = diffuseAlbedo.a;

	return litColor;
}

#endif // __BLINNPHONG_HLSL__