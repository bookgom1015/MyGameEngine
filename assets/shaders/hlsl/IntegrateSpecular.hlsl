#ifndef __INTEGRATESPECULAR_HLSL__
#define __INTEGRATESPECULAR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

#include "BRDF.hlsli"

ConstantBuffer<ConstantBuffer_Pass> cb_Pass	: register(b0);

Texture2D<ToneMapping::IntermediateMapFormat>			gi_BackBuffer	: register(t0);
Texture2D<GBuffer::AlbedoMapFormat>						gi_Albedo		: register(t1);
Texture2D<GBuffer::NormalMapFormat>						gi_Normal		: register(t2);
Texture2D<DepthStencilBuffer::BufferFormat>				gi_Depth		: register(t3);
Texture2D<GBuffer::RMSMapFormat>						gi_RMS			: register(t4);
Texture2D<GBuffer::PositionMapFormat>					gi_Position		: register(t5);
Texture2D<SSAO::AOCoefficientMapFormat>					gi_AOCoeiff		: register(t6);
TextureCube<IrradianceMap::PrefilteredEnvCubeMapFormat>	gi_Prefiltered	: register(t7);
Texture2D<IrradianceMap::IntegratedBrdfMapFormat>		gi_BrdfLUT		: register(t8);
Texture2D<SSR::SSRMapFormat>							gi_Reflection	: register(t9);

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
	float4 ph = mul(vout.PosH, cb_Pass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target{
	const float4 posW = gi_Position.Sample(gsamLinearClamp, pin.TexC);
	const float3 radiance = gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC).rgb;

	if (posW.w == GBuffer::InvalidPositionValueW) return float4(radiance, 1);

	const float3 normalW = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);

	const float4 albedo = gi_Albedo.Sample(gsamAnisotropicWrap, pin.TexC);
	const float3 viewW = normalize(cb_Pass.EyePosW - posW.xyz);

	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamLinearClamp, pin.TexC).rgb;
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float3 reflectedW = reflect(-viewW, normalW);
	const float MaxMipLevel = 5;

	const float3 prefilteredColor = gi_Prefiltered.SampleLevel(gsamLinearClamp, reflectedW, roughness * MaxMipLevel).rgb;

	const float NdotV = max(dot(normalW, viewW), 0);

	const float4 reflection = gi_Reflection.Sample(gsamLinearClamp, pin.TexC);

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * specular, albedo.rgb, metalic);

	const float3 kS = FresnelSchlickRoughness(saturate(dot(normalW, viewW)), fresnelR0, roughness);
	const float3 kD = 1 - kS;

	const float2 envBRDF = gi_BrdfLUT.Sample(gsamLinearClamp, float2(NdotV, roughness));
	const float3 specBias = (kS * envBRDF.x + envBRDF.y);
	const float3 specRadiance = prefilteredColor;
	const float3 reflectionRadiance = reflection.rgb;
	const float specAlpha = reflection.a;

	float3 integratedSpecRadiance = (1 - specAlpha) * specRadiance + specAlpha * reflectionRadiance;
	integratedSpecRadiance *= specBias;

	const float aoCoeff = gi_AOCoeiff.Sample(gsamLinearClamp, pin.TexC);

	return float4(radiance + aoCoeff * integratedSpecRadiance, 1);
}

#endif // __INTEGRATESPECULAR_HLSL__