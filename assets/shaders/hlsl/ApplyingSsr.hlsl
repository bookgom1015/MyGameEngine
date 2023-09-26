#ifndef __APPLYINGSSR_HLSL__
#define __APPLYINGSSR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "ShadingHelpers.hlsli"

ConstantBuffer<PassConstants> cbPass : register(b0);

Texture2D<float3>	gi_BackBuffer	: register(t0);
Texture2D<float4>	gi_Albedo		: register(t1);
Texture2D<float3>	gi_Normal		: register(t2);
Texture2D<float>	gi_Depth		: register(t3);
Texture2D<float3>	gi_RMS			: register(t4);
Texture2D<float4>	gi_Ssr			: register(t5);
TextureCube<float3>	gi_Environment	: register(t6);

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

	const float4 albedo = gi_Albedo.Sample(gsamAnisotropicWrap, pin.TexC);
	const float3 normal = normalize(gi_Normal.Sample(gsamAnisotropicWrap, pin.TexC));

	float4 ssr = gi_Ssr.Sample(gsamLinearClamp, pin.TexC);
	float3 radiance = gi_BackBuffer.Sample(gsamLinearClamp, pin.TexC);
	float k = ssr.a;

	const float3 roughnessMetalicSpecular = gi_RMS.Sample(gsamAnisotropicWrap, pin.TexC);
	const float roughness = roughnessMetalicSpecular.r;
	const float metalic = roughnessMetalicSpecular.g;
	const float specular = roughnessMetalicSpecular.b;

	const float shiness = 1 - roughness;
	const float3 fresnelR0 = lerp((float3)0.08 * specular, albedo.rgb, metalic);

	const float3 V = normalize(cbPass.EyePosW - posW.xyz);
	const float3 L = reflect(-V, normal);
	const float3 lookup = BoxCubeMapLookup(posW.xyz, L, (float3)0, (float3)100);
	float3 const env = gi_Environment.Sample(gsamLinearWrap, lookup);

	const float3 H = normalize(V + L);
	const float3 kS = FresnelSchlick(saturate(dot(H, V)), fresnelR0);
	const float3 kD = 1 - kS;

	const float skyFactor = ceil(max(1 - dot(normal, -V), 0));
	const float3 reflectedLight = shiness * skyFactor * kS * ssr.rgb;

	float3 applied = radiance + reflectedLight;

	return float4(applied, 1);
}

#endif // __APPLYINGSSR_HLSL__