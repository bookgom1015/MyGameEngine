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
Texture2D<float3>	gi_Normal			: register(t1);
Texture2D<float>	gi_Depth			: register(t2);
Texture2D<float4>	gi_Specular			: register(t3);
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
	float3 posV = (pz / pin.PosV.z) * pin.PosV;
	float4 posW = mul(float4(posV, 1), cb.InvView);
	
	//
	// Diffuse reflection
	//
	float4 color = gi_Color.Sample(gsamAnisotropicWrap, pin.TexC);

	float4 ssaoPosH = mul(posW, cb.ViewProjTex);
	ssaoPosH /= ssaoPosH.w;
	float ambientAccess = gi_AOCoefficient.Sample(gsamAnisotropicWrap, ssaoPosH.xy, 0);

	float4 ambient = color * ambientAccess * cb.AmbientLight;
	
	const float4 F0 = gi_Specular.Sample(gsamAnisotropicWrap, pin.TexC);
	const float shiness = 1.0f - F0.a;
	Material mat = { color, F0.rgb, shiness };

	float3 shadowFactor = 0;
	float4 shadowPosH = mul(posW, cb.ShadowTransform);
	shadowFactor[0] = CalcShadowFactor(gi_Shadow, gsamShadow, shadowPosH);
	
	float3 normalW = normalize(gi_Normal.Sample(gsamAnisotropicWrap, pin.TexC));
	float3 toEyeW = normalize(cb.EyePosW - posW.xyz);
	float4 directLight = max(ComputeLighting(cb.Lights, mat, posW, normalW, toEyeW, shadowFactor), (float4)0);
	
	float4 diffuse = ambient + directLight;
	diffuse.a = color.a;

	//
	// Specular reflection
	//
	float3 toLight = reflect(-toEyeW, normalW);
	float3 fresnelFactor = SchlickFresnel(F0.rgb, normalW, toLight);

	float3 lookup = BoxCubeMapLookup(posW.xyz, toLight, (float3)0.0f, (float3)100.0f);
	float3 envColor = gi_SkyCube.Sample(gsamLinearWrap, lookup);

	float specIntensity = shiness * fresnelFactor;
	float4 specular = float4(specIntensity * envColor, specIntensity);

	PixelOut pout = (PixelOut)0;
	pout.BackBuffer = diffuse + specular;
	pout.Diffuse = diffuse;
	pout.Specular = specular;
	return pout;
}

#endif // __BLINNPHONG_HLSL__