#ifndef __BRDF_HLSL__
#define __BRDF_HLSL__

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

#include "BRDF.hlsli"

ConstantBuffer<PassConstants> cbPass	: register(b0);

Texture2D<float4>	gi_Albedo			: register(t0);
Texture2D<float3>	gi_Normal			: register(t1);
Texture2D<float>	gi_Depth			: register(t2);
Texture2D<float3>	gi_RMS				: register(t3);
Texture2D<float>	gi_Shadow			: register(t4);
Texture2D<float>	gi_AOCoeiff			: register(t5);
TextureCube<float3>	gi_Diffuse			: register(t6);
TextureCube<float3>	gi_Prefiltered		: register(t7);
Texture2D<float2>	gi_BrdfLUT			: register(t8);

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

float4 PS(VertexOut pin) : SV_Target {
	// Get viewspace normal and z-coord of this pixel.  
	float pz = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	pz = NdcDepthToViewDepth(pz, cbPass.Proj);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	const float3 posV = (pz / pin.PosV.z) * pin.PosV;
	const float4 posW = mul(float4(posV, 1), cbPass.InvView);
	
	const float4 albedo = gi_Albedo.Sample(gsamAnisotropicWrap, pin.TexC);

	float4 ssaoPosH = mul(posW, cbPass.ViewProjTex);
	ssaoPosH /= ssaoPosH.w;
	
	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamAnisotropicWrap, pin.TexC);
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * specular, albedo.rgb, metalic);

	Material mat = { albedo, fresnelR0, shiness, metalic };

	float3 shadowFactor = 0;
#ifdef DXR
	shadowFactor[0] = gi_Shadow.Sample(gsamPointClamp, pin.TexC);
#else
	const float4 shadowPosH = mul(posW, cbPass.ShadowTransform);
	shadowFactor[0] = CalcShadowFactor(gi_Shadow, gsamShadow, shadowPosH);
#endif
	
	const float3 normalW = normalize(gi_Normal.Sample(gsamAnisotropicWrap, pin.TexC));
	const float3 viewW = normalize(cbPass.EyePosW - posW.xyz);
	const float3 radiance = max(ComputeBRDF(cbPass.Lights, mat, posW.xyz, normalW, viewW, shadowFactor), (float3)0);

	const float3 kS = FresnelSchlickRoughness(saturate(dot(normalW, viewW)), fresnelR0, roughness);
	const float3 kD = 1 - kS;

	const float3 diffSamp = gi_Diffuse.SampleLevel(gsamLinearClamp, normalW, 0);
	const float3 diffuseIrradiance = diffSamp * albedo;

	const float3 reflectedW = reflect(-viewW, normalW);
	const float MaxMipLevel = 5;

	const float3 prefilteredColor = gi_Prefiltered.SampleLevel(gsamLinearClamp, reflectedW, roughness * MaxMipLevel);
	
	const float NdotV = max(dot(normalW, viewW), 0);

	const float2 envBRDF =  gi_BrdfLUT.SampleLevel(gsamLinearClamp, float2(NdotV, roughness), 0);
	const float3 specIrradiance = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

	const float aoCoeiff = gi_AOCoeiff.SampleLevel(gsamLinearClamp, pin.TexC, 0);

	const float3 ambient = (kD * diffuseIrradiance + specIrradiance) * aoCoeiff;

	return float4(radiance + ambient, albedo.a);
}

#endif // __BRDF_HLSL__