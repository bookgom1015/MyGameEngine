#ifndef __SHADOW_HLSLI__
#define __SHADOW_HLSLI__

float CalcShadowFactorPCF(Texture2D<float> shadowMap, SamplerComparisonState sampComp, float4 shadowPosH) {
	shadowPosH.xyz /= shadowPosH.w;

	float depth = shadowPosH.z;

	uint width, height, numMips;
	shadowMap.GetDimensions(0, width, height, numMips);

	float dx = 1.0f / (float)width;

	float percentLit = 0.0f;
	const float2 offsets[9] = {
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
	};

	[unroll]
	for (int i = 0; i < 9; ++i) {
		percentLit += shadowMap.SampleCmpLevelZero(sampComp, shadowPosH.xy + offsets[i], depth);
	}

	return percentLit / 9.0f;
}

float CalcShadowFactorDirectional(Texture2D<float> shadowMap, SamplerComparisonState sampComp, float4x4 viewProjTex, float3 fragPosW) {
	float4 shadowPosH = mul(float4(fragPosW, 1), viewProjTex);
	shadowPosH.xyz /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return shadowMap.SampleCmpLevelZero(sampComp, shadowPosH.xy, depth);
}

//float CalcShadowFactorSpot(Texture2D<float> shadowMap, SamplerComparisonState sampComp, float4x4 shadowViewProj float4x4 cubeViewProj, float3 fragPosW, float3 lightPosW) {
//	const float3 direction = normalize(fragPosW - lightPosW);
//
//	const float closestDepth = shadowMap.Sample(samp, direction);
//
//	float4 shadowPosH = mul(float4(fragPosW, 1), viewProj);
//	shadowPosH /= shadowPosH.w;
//
//	const float depth = shadowPosH.z;
//
//	return depth < closestDepth;
//
//
//	float4 shadowPosH = mul(float4(fragPosW, 1), viewProj);
//	shadowPosH.xyz /= shadowPosH.w;
//
//	const float depth = shadowPosH.z;
//
//	float2 texc = shadowPosH.xy * 0.5 + (float2)0.5;
//	texc.y = 1 - texc.y;
//
//	return shadowMap.SampleCmpLevelZero(sampComp, texc, depth);
//}

float CalcShadowFactorPoint(TextureCube<float> shadowMap, SamplerState samp, float4x4 viewProj, float3 fragPosW, float3 lightPosW) {
	const float3 direction = normalize(fragPosW - lightPosW);

	const float closestDepth = shadowMap.Sample(samp, direction);

	float4 shadowPosH = mul(float4(fragPosW, 1), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depth < closestDepth;
}

float CalcShadowFactorPointCS(Texture2DArray<float> shadowMap, SamplerState samp, float4x4 viewProj, float3 fragPosW, float2 uv, uint index) {
	const float closestDepth = shadowMap.SampleLevel(samp, float3(uv, index), 0);
	if (closestDepth == 1) return 1;

	float4 shadowPosH = mul(float4(fragPosW, 1), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depth < closestDepth;
}

uint CalcShiftedShadowValueF(float percent, uint value, uint index) {
	uint shadowFactor = percent < 0.5 ? 0 : 1;
	uint shifted = shadowFactor << index;

	return value | shifted;
}

uint CalcShiftedShadowValueB(bool isHit, uint value, uint index) {
	uint shadowFactor = isHit ? 0 : 1;
	uint shifted = shadowFactor << index;

	return value | shifted;
}

uint GetShiftedShadowValue(uint value, uint index) {
	return (value >> index) & 1;
}

#endif // __SHADOW_HLSLI__