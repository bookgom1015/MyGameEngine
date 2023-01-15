#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

#include "LightingUtil.hlsli"
#include "Samplers.hlsli"

#define NUM_TEXTURE_MAPS 64

cbuffer cbPerObject : register(b0) {
	float4x4 gWorld;
	float4x4 gPrevWorld;
	float4x4 gTexTransform;
};

cbuffer cbPass : register(b1) {
	float4x4	gView;
	float4x4	gInvView;
	float4x4	gProj;
	float4x4	gInvProj;
	float4x4	gViewProj;
	float4x4	gInvViewProj;
	float4x4	gPrevViewProj;
	float4x4	gViewProjTex;
	float4x4	gShadowTransform;
	float3		gEyePosW;
	float		gPassConstantsPad0;
	float2		gJitteredOffset;
	float		gPassConstantsPad1;
	float		gPassConstantsPad2;
	float4		gAmbientLight;
	Light		gLights[MaxLights];
};

cbuffer cbMaterial : register(b2) {
	float4		gDiffuseAlbedo;
	float3		gFresnelR0;
	float		gRoughness;
	float4x4	gMatTransform;
	int			gDiffuseSrvIndex;
	int			gNormalSrvIndex;
	int			gAlphaSrvIndex;
	float		gMatConstPad0;
};

TextureCube gCubeMap							: register(t0);
Texture2D	gColorMap							: register(t1);
Texture2D	gAlbedoMap							: register(t2);
Texture2D	gNormalMap							: register(t3);
Texture2D	gDepthMap							: register(t4);
Texture2D	gSpecularMap						: register(t5);
Texture2D	gVelocityMap						: register(t6);
Texture2D	gShadowMap							: register(t7);
Texture2D	gAmbientMap0						: register(t8);
Texture2D	gAmbientMap1						: register(t9);
Texture2D	gRandomVectorMap					: register(t10);	
Texture2D	gResolveMap							: register(t11);
Texture2D	gHistoryMap							: register(t12);
Texture2D	gBackBuffer0						: register(t13);
Texture2D	gBackBuffer1						: register(t14);
Texture2D	gCocMap								: register(t15);
Texture2D	gDofMap								: register(t16);
Texture2D	gDofBlurMap							: register(t17);
Texture2D	gFont								: register(t18);
Texture2D	gTextureMap[NUM_TEXTURE_MAPS]		: register(t19);

float CalcShadowFactor(float4 shadowPosH) {
	shadowPosH.xyz /= shadowPosH.w;

	float depth = shadowPosH.z;

	uint width, height, numMips;
	gShadowMap.GetDimensions(0, width, height, numMips);

	float dx = 1.0f / (float)width;

	float percentLit = 0.0f;
	const float2 offsets[9] = {
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
	};

	[unroll]
	for (int i = 0; i < 9; ++i) {
		percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
	}

	return percentLit / 9.0f;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

float2 CalcVelocity(float4 curr_pos, float4 prev_pos) {
	curr_pos.xy = (curr_pos.xy + (float2)1.0f) / 2.0f;
	curr_pos.y = 1.0f - curr_pos.y;

	prev_pos.xy = (prev_pos.xy + (float2)1.0f) / 2.0f;
	prev_pos.y = 1.0f - prev_pos.y;

	return (curr_pos - prev_pos).xy;
}

#endif // __COMMON_HLSLI__
