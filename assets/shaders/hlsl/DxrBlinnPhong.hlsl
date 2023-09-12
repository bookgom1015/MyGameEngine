#ifndef __DXRBLINNPHONG_HLSL__
#define __DXRBLINNPHONG_HLSL__

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

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "LightingUtil.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants> cbPass : register(b0);

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
	VertexOut vout = (VertexOut)0.0f;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, cbPass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
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

	const float ambientAccess = gi_AOCoefficient.Sample(gsamAnisotropicWrap, ssaoPosH.xy, 0);
	const float3 ambient = albedo.rgb * ambientAccess * cbPass.AmbientLight.rgb;

	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamAnisotropicWrap, pin.TexC);
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * specular, albedo.rgb, metalic);

	Material mat = { albedo, fresnelR0, shiness };

	float3 shadowFactor = (float3)1.0f;
	shadowFactor[0] = gi_Shadow.Sample(gsamPointClamp, pin.TexC);

	const float3 normalW = normalize(gi_Normal.Sample(gsamAnisotropicWrap, pin.TexC));
	const float3 toEyeW = normalize(cbPass.EyePosW - posW.xyz);
	const float3 brdf = max(ComputeBRDF(cbPass.Lights, mat, posW.xyz, normalW, toEyeW, shadowFactor), (float3)0);

	float4 radiance = float4(ambient + brdf, 0);
	radiance.a = albedo.a;

	return radiance;
}

#endif // __DXRBLINNPHONG_HLSL__