#ifndef __SHADOW_HLSLI__
#define __SHADOW_HLSLI__

float CalcShadowFactorPCF(Texture2D<float> depthMap, SamplerComparisonState sampComp, float4 shadowPosH) {
	shadowPosH.xyz /= shadowPosH.w;

	float depth = shadowPosH.z;

	uint width, height, numMips;
	depthMap.GetDimensions(0, width, height, numMips);

	float dx = 1.0f / (float)width;

	float percentLit = 0.0f;
	const float2 offsets[9] = {
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
	};

	[unroll]
	for (int i = 0; i < 9; ++i) {
		percentLit += depthMap.SampleCmpLevelZero(sampComp, shadowPosH.xy + offsets[i], depth);
	}

	return percentLit / 9.0f;
}

float CalcShadowFactor(Texture2D<float> depthMap, SamplerComparisonState sampComp, float4x4 viewProj, float3 fragPosW) {
	float4 shadowPosH = mul(float4(fragPosW, 1), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;
	const float2 texc = shadowPosH.xy * float2(0.5, -0.5) + (float2)0.5;

	return depthMap.SampleCmpLevelZero(sampComp, texc, depth);
}

float CalcShadowFactorDirectional(Texture2D<float> depthMap, SamplerComparisonState sampComp, float4x4 viewProjTex, float3 fragPosW) {
	float4 shadowPosH = mul(float4(fragPosW, 1), viewProjTex);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depthMap.SampleCmpLevelZero(sampComp, shadowPosH.xy, depth);
}

float CalcShadowFactorCube(TextureCube<float> depthMap, SamplerComparisonState sampComp, float4x4 viewProj, float3 fragPosW, float3 lightPosW) {
	const float3 direction = normalize(fragPosW - lightPosW);

	float4 shadowPosH = mul(float4(fragPosW, 1), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depthMap.SampleCmpLevelZero(sampComp, direction, depth);
}

float CalcShadowFactorCubeCS(Texture2DArray<float> depthMap, SamplerComparisonState sampComp, float4x4 viewProj, float3 fragPosW, float2 uv, uint index) {
	float4 shadowPosH = mul(float4(fragPosW, 1), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depthMap.SampleCmpLevelZero(sampComp, float3(uv, index), depth);
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