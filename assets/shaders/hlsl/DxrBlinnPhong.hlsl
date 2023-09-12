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

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "LightingUtil.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<PassConstants> cbPass : register(b0);

Texture2D<float4>	gi_Color	: register(t0);
Texture2D<float4>	gi_Albedo	: register(t1);
Texture2D<float3>	gi_Normal	: register(t2);
Texture2D<float>	gi_Depth	: register(t3);
Texture2D<float4>	gi_Specular	: register(t4);
Texture2D<float>	gi_Shadow	: register(t5);
Texture2D<float>	gi_AOCeff	: register(t6);

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
	float3 posV = (pz / pin.PosV.z) * pin.PosV;
	float4 posW = mul(float4(posV, 1.0f), cbPass.InvView);

	float3 normalW = normalize(gi_Normal.Sample(gsamPointClamp, pin.TexC));
	float3 toEyeW = normalize(cbPass.EyePosW - posW.xyz);

	float4 colorSample = gi_Color.Sample(gsamPointClamp, pin.TexC);
	float4 albedoSample = gi_Albedo.Sample(gsamPointClamp, pin.TexC);
	float4 diffuseAlbedo = colorSample * albedoSample;

	float ambientAccess;
	{
		uint width, height;
		gi_AOCeff.GetDimensions(width, height);

		uint2 index = uint2(pin.TexC.x * width - 0.5f, pin.TexC.y * height - 0.5f);

		ambientAccess = gi_AOCeff[index];
	}

	float4 ambient = ambientAccess * cbPass.AmbientLight * diffuseAlbedo;

	float4 specular = gi_Specular.Sample(gsamPointClamp, pin.TexC);
	const float shiness = 1.0f - specular.a;
	Material mat = { albedoSample, specular.rgb, shiness };

	float3 shadowFactor = (float3)1.0f;
	shadowFactor[0] = gi_Shadow.Sample(gsamPointClamp, pin.TexC);

	float3 directLight = ComputeLighting(cbPass.Lights, mat, posW.xyz, normalW, toEyeW, shadowFactor);

	float4 litColor = float4(ambient + directLight, 0);
	litColor.a = diffuseAlbedo.a;

	return litColor;
}

#endif // __DXRBLINNPHONG_HLSL__