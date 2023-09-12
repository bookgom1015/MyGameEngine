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

#ifndef BLINN_PHONG
#define BLINN_PHONG
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<PassConstants> cb		: register(b0);

Texture2D<float4>	gi_Albedo			: register(t0);
Texture2D<float3>	gi_Normal			: register(t1);
Texture2D<float>	gi_Depth			: register(t2);
Texture2D<float3>	gi_RMS				: register(t3);
Texture2D<float>	gi_Shadow			: register(t4);
Texture2D<float>	gi_AOCoefficient	: register(t5);
TextureCube<float4>	gi_SkyCube			: register(t6);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

struct PixelOut {
	float4 BackBuffer	: SV_TARGET0;
	float4 Diffuse		: SV_TARGET1;
	float4 Specular		: SV_TARGET2;
	float4 Reflectivity	: SV_TARGET3;
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

PixelOut PS(VertexOut pin) : SV_Target{
	// Get viewspace normal and z-coord of this pixel.  
	float pz = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	pz = NdcDepthToViewDepth(pz, cb.Proj);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	const float3 posV = (pz / pin.PosV.z) * pin.PosV;
	const float4 posW = mul(float4(posV, 1), cb.InvView);
	
	//
	// Diffuse reflection
	//
	const float4 albedo = gi_Albedo.Sample(gsamAnisotropicWrap, pin.TexC);

	float4 ssaoPosH = mul(posW, cb.ViewProjTex);
	ssaoPosH /= ssaoPosH.w;

	const float ambientAccess = gi_AOCoefficient.Sample(gsamAnisotropicWrap, ssaoPosH.xy, 0);
	const float3 ambient = albedo.rgb * ambientAccess * cb.AmbientLight.rgb;
	
	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamAnisotropicWrap, pin.TexC);
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * specular, albedo.rgb, metalic);

	Material mat = { albedo, fresnelR0, shiness };

	float3 shadowFactor = 0;
	const float4 shadowPosH = mul(posW, cb.ShadowTransform);
	shadowFactor[0] = CalcShadowFactor(gi_Shadow, gsamShadow, shadowPosH);
	
	const float3 normalW = normalize(gi_Normal.Sample(gsamAnisotropicWrap, pin.TexC));
	const float3 toEyeW = normalize(cb.EyePosW - posW.xyz);
	const float3 directLight = max(ComputeLighting(cb.Lights, mat, posW.xyz, normalW, toEyeW, shadowFactor), (float3)0);
	
	float4 diffuseReflectance = float4(ambient + directLight, 0);
	diffuseReflectance.a = albedo.a;

	//
	// Specular reflection
	//
	const float3 toLight = reflect(-toEyeW, normalW);
	const float3 fresnelFactor = SchlickFresnel(fresnelR0, normalW, toLight);

	const float3 lookup = BoxCubeMapLookup(posW.xyz, toLight, (float3)0, (float3)100);
	const float3 envColor = gi_SkyCube.Sample(gsamLinearWrap, lookup).rgb;

	const float reflectivity = shiness * fresnelFactor;
	const float4 specularReflectance = float4(reflectivity * envColor, 0);

	PixelOut pout = (PixelOut)0;
	pout.BackBuffer = diffuseReflectance + specularReflectance;
	pout.Diffuse = diffuseReflectance;
	pout.Specular = specularReflectance;
	pout.Reflectivity = reflectivity;

	return pout;
}

#endif // __BLINNPHONG_HLSL__