#ifndef __BRDF_HLSL__
#define __BRDF_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "BRDF.hlsli"
#include "Shadow.hlsli"

ConstantBuffer<ConstantBuffer_Pass>						cb_Pass					: register(b0);

Texture2D<GBuffer::AlbedoMapFormat>						gi_Albedo				: register(t0);
Texture2D<GBuffer::NormalMapFormat>						gi_Normal				: register(t1);
Texture2D<GBuffer::DepthMapFormat>						gi_Depth				: register(t2);
Texture2D<GBuffer::RMSMapFormat>						gi_RMS					: register(t3);
Texture2D<GBuffer::PositionMapFormat>					gi_Position				: register(t4);
TextureCube<IrradianceMap::DiffuseIrradCubeMapFormat>	gi_DiffuseIrrad			: register(t5);

#ifdef DXR
Texture2D<RTAO::AOCoefficientMapFormat>					gi_AOCoeiff				: register(t0, space1);
Texture2D<DXR_Shadow::ShadowMapFormat>					gi_Shadow				: register(t1, space1);
#else
Texture2D<SSAO::AOCoefficientMapFormat>					gi_AOCoeiff				: register(t0, space1);
Texture2D<Shadow::ShadowMapFormat>						gi_Shadow				: register(t1, space1);
#endif

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout = (VertexOut)0;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	const float4 ph = mul(vout.PosH, cb_Pass.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {	
	const float4 posW = gi_Position.Sample(gsamLinearClamp, pin.TexC);

	float4 ssaoPosH = mul(posW, cb_Pass.ViewProjTex);
	ssaoPosH /= ssaoPosH.w;
	
	const float4 albedo = gi_Albedo.Sample(gsamLinearClamp, pin.TexC);

	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamLinearClamp, pin.TexC).xyz;
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08f * specular, albedo.rgb, metalic);

	Material mat = { albedo, fresnelR0, shiness, metalic };

	float shadowFactor[MaxLights];
	{
		[unroll]
		for (uint i = 0; i < MaxLights; ++i) shadowFactor[i] = 1;
	}

	{
		uint2 size;
		gi_Shadow.GetDimensions(size.x, size.y);

		const uint2 id = pin.TexC * size - 0.5;
		const uint value = gi_Shadow.Load(uint3(id, 0));

		[loop]
		for (uint i = 0; i < cb_Pass.LightCount; ++i) {
			shadowFactor[i] = GetShiftedShadowValue(value, i);
		}
	}

	const float3 normalW = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);

	const float3 viewW = normalize(cb_Pass.EyePosW - posW.xyz);
	const float3 radiance = max(ComputeBRDF(cb_Pass.Lights, mat, posW.xyz, normalW, viewW, shadowFactor, cb_Pass.LightCount), (float3)0.f);

	const float3 kS = FresnelSchlickRoughness(saturate(dot(normalW, viewW)), fresnelR0, roughness);
	const float3 kD = 1.f - kS;

	const float3 diffIrrad = gi_DiffuseIrrad.SampleLevel(gsamLinearClamp, normalW, 0).rgb;
	const float3 diffuse = diffIrrad * albedo.rgb;

	const float aoCoeiff = gi_AOCoeiff.SampleLevel(gsamLinearClamp, pin.TexC, 0);

	const float3 ambient = (kD * diffuse) * aoCoeiff;

	return float4(radiance + ambient, albedo.a);
}

#endif // __BRDF_HLSL__