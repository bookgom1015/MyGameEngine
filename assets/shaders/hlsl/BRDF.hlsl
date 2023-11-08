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

Texture2D<GBuffer::AlbedoMapFormat>			gi_Albedo	: register(t0);
Texture2D<GBuffer::NormalMapFormat>			gi_Normal	: register(t1);
Texture2D<GBuffer::DepthMapFormat>			gi_Depth	: register(t2);
Texture2D<GBuffer::RMSMapFormat>			gi_RMS		: register(t3);
Texture2D<ShadowMap::ShadowMapFormat>		gi_Shadow	: register(t4);
Texture2D<Ssao::AOCoefficientMapFormat>		gi_AOCoeiff	: register(t5);
TextureCube<float3>							gi_Diffuse	: register(t6);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	float4 ph = mul(vout.PosH, cbPass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target {
	const float3 normalW = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);

	float depth = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	depth = NdcDepthToViewDepth(depth, cbPass.Proj);

	const float3 posV = (depth / pin.PosV.z) * pin.PosV;
	const float4 posW = mul(float4(posV, 1), cbPass.InvView);
	
	const float4 albedo = gi_Albedo.Sample(gsamAnisotropicWrap, pin.TexC);

	float4 ssaoPosH = mul(posW, cbPass.ViewProjTex);
	ssaoPosH /= ssaoPosH.w;
	
	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamAnisotropicWrap, pin.TexC).xyz;
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

	const float3 viewW = normalize(cbPass.EyePosW - posW.xyz);
	const float3 radiance = max(ComputeBRDF(cbPass.Lights, mat, posW.xyz, normalW, viewW, shadowFactor), (float3)0);

	const float3 kS = FresnelSchlickRoughness(saturate(dot(normalW, viewW)), fresnelR0, roughness);
	const float3 kD = 1 - kS;

	const float3 diffSamp = gi_Diffuse.SampleLevel(gsamLinearClamp, normalW, 0);
	const float3 diffuseIrradiance = diffSamp * albedo.rgb;

	const float aoCoeiff = gi_AOCoeiff.SampleLevel(gsamLinearClamp, pin.TexC, 0);

	const float3 ambient = (kD * diffuseIrradiance) * aoCoeiff;

	return float4(radiance + ambient, albedo.a);
}

#endif // __BRDF_HLSL__