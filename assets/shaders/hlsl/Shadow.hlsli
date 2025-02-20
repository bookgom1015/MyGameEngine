#ifndef __SHADOW_HLSLI__
#define __SHADOW_HLSLI__

float CalcShadowFactorPCF(
		in Texture2D<float> depthMap, 
		in SamplerComparisonState sampComp, 
		in float4 shadowPosH) {
	shadowPosH.xyz /= shadowPosH.w;
	const float depth = shadowPosH.z;

	uint2 dims;
	depthMap.GetDimensions(dims.x, dims.y);

	const float dx = 1.f / (float)dims.x;

	float percentLit = 0.f;
	const float2 offsets[9] = {
		float2(-dx, -dx), float2(0.f, -dx), float2(dx, -dx),
		float2(-dx, 0.f), float2(0.f, 0.f), float2(dx, 0.f),
		float2(-dx, +dx), float2(0.f, +dx), float2(dx, +dx)
	};

	[unroll]
	for (uint i = 0; i < 9; ++i) {
		percentLit += depthMap.SampleCmpLevelZero(sampComp, shadowPosH.xy + offsets[i], depth);
	}

	return percentLit / 9.f;
}

float CalcShadowFactor(
		in Texture2D<float> depthMap, 
		in SamplerComparisonState sampComp, 
		in float4x4 viewProj, 
		in float3 fragPosW) {
	float4 shadowPosH = mul(float4(fragPosW, 1.f), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;
	const float2 texc = shadowPosH.xy * float2(0.5f, -0.5f) + (float2)0.5f;

	return depthMap.SampleCmpLevelZero(sampComp, texc, depth);
}

float CalcShadowFactorDirectional(
		in Texture2D<float> depthMap, 
		in SamplerComparisonState sampComp, 
		in float4x4 viewProjTex, 
		in float3 fragPosW) {
	float4 shadowPosH = mul(float4(fragPosW, 1.f), viewProjTex);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depthMap.SampleCmpLevelZero(sampComp, shadowPosH.xy, depth);
}

float CalcShadowFactorCube(
		in TextureCube<float> depthMap, 
		in SamplerComparisonState sampComp, 
		in float4x4 viewProj, 
		in float3 fragPosW, 
		in float3 lightPosW) {
	const float3 direction = normalize(fragPosW - lightPosW);

	float4 shadowPosH = mul(float4(fragPosW, 1.f), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depthMap.SampleCmpLevelZero(sampComp, direction, depth);
}

float CalcShadowFactorCubeCS(
		in Texture2DArray<float> depthMap, 
		in SamplerComparisonState sampComp, 
		in float4x4 viewProj, 
		in float3 fragPosW, 
		in float2 uv, 
		in uint index) {
	float4 shadowPosH = mul(float4(fragPosW, 1.f), viewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depthMap.SampleCmpLevelZero(sampComp, float3(uv, index), depth);
}

uint CalcShiftedShadowValueF(
		in float percent, 
		in uint value, 
		in uint index) {
	const uint shadowFactor = percent < 0.5f ? 0 : 1;
	const uint shifted = shadowFactor << index;

	return value | shifted;
}

uint CalcShiftedShadowValueB(
		in bool isHit, 
		in uint value, 
		in uint index) {
	const uint shadowFactor = isHit ? 0 : 1;
	const uint shifted = shadowFactor << index;

	return value | shifted;
}

uint GetShiftedShadowValue(
		in uint value, 
		in uint index) {
	return (value >> index) & 1;
}

#endif // __SHADOW_HLSLI__