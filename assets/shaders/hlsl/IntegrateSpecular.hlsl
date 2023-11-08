#ifndef __INTEGRATESPECULAR_HLSL__
#define __INTEGRATESPECULAR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

#include "BRDF.hlsli"

ConstantBuffer<PassConstants> cbPass	: register(b0);

Texture2D<float3>						gi_BackBuffer	: register(t0);
Texture2D<GBuffer::AlbedoMapFormat>		gi_Albedo		: register(t1);
Texture2D<GBuffer::NormalMapFormat>		gi_Normal		: register(t2);
Texture2D<float>						gi_Depth		: register(t3);
Texture2D<GBuffer::RMSMapFormat>		gi_RMS			: register(t4);
Texture2D<Ssao::AOCoefficientMapFormat>	gi_AOCoeiff		: register(t5);
TextureCube<float3>						gi_Prefiltered	: register(t6);
Texture2D<float2>						gi_BrdfLUT		: register(t7);
Texture2D<float4>						gi_Ssr			: register(t8);

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
	float pz = gi_Depth.Sample(gsamDepthMap, pin.TexC);
	pz = NdcDepthToViewDepth(pz, cbPass.Proj);

	const float3 posV = (pz / pin.PosV.z) * pin.PosV;
	const float4 posW = mul(float4(posV, 1), cbPass.InvView);

	const float3 normalW = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);

	const float4 albedo = gi_Albedo.Sample(gsamAnisotropicWrap, pin.TexC);
	const float3 viewW = normalize(cbPass.EyePosW - posW.xyz);

	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamAnisotropicWrap, pin.TexC).rgb;
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float3 reflectedW = reflect(-viewW, normalW);
	const float MaxMipLevel = 5;

	const float3 prefilteredColor = gi_Prefiltered.SampleLevel(gsamLinearClamp, reflectedW, roughness * MaxMipLevel);

	const float NdotV = max(dot(normalW, viewW), 0);

	const float4 ssr = gi_Ssr.Sample(gsamLinearClamp, pin.TexC);
	const float3 radiance = gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC);
	const float k = ssr.a;

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * specular, albedo.rgb, metalic);

	const float3 lightW = reflect(-viewW, normalW);
	const float3 halfW = normalize(viewW + lightW);

	const float3 kS = FresnelSchlickRoughness(saturate(dot(halfW, viewW)), fresnelR0, roughness);
	const float3 kD = 1 - kS;

	const float2 envBRDF = gi_BrdfLUT.SampleLevel(gsamLinearClamp, float2(NdotV, roughness), 0);
	const float3 specRadiance = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

	const float3 ssrRadiance = shiness * (kS * envBRDF.x + envBRDF.y) * ssr.rgb;

	const float t = k * shiness;
	const float3 integratedSpecRadiance = (1 - t) * specRadiance + t * ssrRadiance;
	const float aoCoeff = gi_AOCoeiff.SampleLevel(gsamLinearClamp, pin.TexC, 0);

	const float skyFactor = ceil(max(1 - dot(normalW, -viewW), 0));

	return float4(radiance + skyFactor * aoCoeff * integratedSpecRadiance, 1);
}

#endif // __INTEGRATESPECULAR_HLSL__