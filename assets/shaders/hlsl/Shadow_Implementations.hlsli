#ifndef __SHADOW_IMPLEMENTATIONS_HLSLI__
#define __SHADOW_IMPLEMENTATIONS_HLSLI__

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

float CalcShadowFactor(Texture2D<float> shadowMap, SamplerComparisonState sampComp, float4 shadowPosH) {
	shadowPosH.xyz /= shadowPosH.w;
	const float depth = shadowPosH.z;

	return shadowMap.SampleCmpLevelZero(sampComp, shadowPosH.xy, depth);
}

float CalcShadowFactorCube(TextureCube<float> shadowMap, SamplerState samp, float4x4 shadowViewProj, float3 fragPosW, float3 lightPosW) {
	const float3 direction = normalize(fragPosW - lightPosW);

	const float closestDepth = shadowMap.SampleLevel(samp, direction, 0);

	float4 shadowPosH = mul(float4(fragPosW, 1), shadowViewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depth < closestDepth;
}

float CalcShadowFactorTexArray(Texture2DArray<float> shadowMap, SamplerState samp, float4x4 shadowViewProj, float3 fragPosW, float2 uv, uint index) {
	const float closestDepth = shadowMap.SampleLevel(samp, float3(uv, index), 0);

	float4 shadowPosH = mul(float4(fragPosW, 1), shadowViewProj);
	shadowPosH /= shadowPosH.w;

	const float depth = shadowPosH.z;

	return depth < closestDepth;
}

uint CalcShiftedShadowValueF(float percent, uint value, uint index) {
	if (index == 0) value = 0;

	uint shadowFactor = percent < 0.5 ? 0 : 1;
	uint shifted = shadowFactor << index;

	return value | shifted;
}

uint CalcShiftedShadowValueB(bool isHit, uint value, uint index) {
	if (index == 0) value = 0;

	uint shadowFactor = isHit ? 0 : 1;
	uint shifted = shadowFactor << index;

	return value | shifted;
}

uint GetShiftedShadowValue(uint value, uint index) {
	return (value >> index) & 1;
}

#endif // __SHADOW_IMPLEMENTATIONS_HLSLI__