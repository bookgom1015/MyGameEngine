#ifndef __APPLYINGSSR_HLSL__
#define __APPLYINGSSR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<PassConstants> cbPass : register(b0);

Texture2D<SDR_Format>								gi_BackBuffer	: register(t0);
Texture2D<GBuffer::AlbedoMapFormat>					gi_Albedo		: register(t1);
Texture2D<GBuffer::NormalMapFormat>					gi_Normal		: register(t2);
Texture2D<GBuffer::DepthMapFormat>					gi_Depth		: register(t3);
Texture2D<GBuffer::RMSMapFormat>					gi_RMS			: register(t4);
Texture2D<Ssr::SsrMapFormat>						gi_Ssr			: register(t5);
Texture2D<IrradianceMap::IntegratedBrdfMapFormat>	gi_BrdfLUT		: register(t6);
TextureCube<IrradianceMap::EnvCubeMapFormat>		gi_Environment	: register(t7);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	float4 ph = mul(vout.PosH, cbPass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	float pz = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	pz = NdcDepthToViewDepth(pz, cbPass.Proj);

	const float3 posV = (pz / pin.PosV.z) * pin.PosV;
	const float4 posW = mul(float4(posV, 1), cbPass.InvView);

	const float4 albedo = gi_Albedo.Sample(gsamLinearClamp, pin.TexC);
	const float3 normalW = gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz;

	const float4 ssr = gi_Ssr.Sample(gsamLinearClamp, pin.TexC);
	const float3 radiance = gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC);
	float k = ssr.a;

	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamAnisotropicWrap, pin.TexC);
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * specular, albedo.rgb, metalic);

	const float3 viewW = normalize(cbPass.EyePosW - posW.xyz);
	const float3 lightW = reflect(-viewW, normalW);
	const float3 lookup = BoxCubeMapLookup(posW.xyz, lightW, (float3)0, (float3)100);
	float3 const env = gi_Environment.Sample(gsamLinearWrap, lookup).rgb;

	const float3 halfW = normalize(viewW + lightW);

	const float2 envBRDF = gi_BrdfLUT.SampleLevel(gsamLinearClamp, float2(saturate(dot(normalW, viewW)), roughness), 0);

	const float3 kS = FresnelSchlick(saturate(dot(halfW, viewW)), fresnelR0);
	const float3 kD = 1 - kS;

	const float skyFactor = ceil(max(1 - dot(normalW, -viewW), 0));
	const float3 reflectedLight = skyFactor * shiness * (kS * envBRDF.x + envBRDF.y) * ssr.rgb;

	float3 applied = radiance + reflectedLight;

	return float4(applied, 1);
}

#endif // __APPLYINGSSR_HLSL__