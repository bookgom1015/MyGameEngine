#ifndef __SHADOW_DECLARATIONS_HLSLI__
#define __SHADOW_DECLARATIONS_HLSLI__

float CalcShadowFactorPCF(Texture2D<float> shadowMap, SamplerComparisonState sampComp, float4 shadowPosH);
float CalcShadowFactor(Texture2D<float> shadowMap, SamplerComparisonState sampComp, float4 shadowPosH);
float CalcShadowFactorCube(TextureCube<float> shadowMap, SamplerState samp, float4x4 shadowViewProj, float3 fragPosW, float3 lightPosW);
float CalcShadowFactorTexArray(Texture2DArray<float> shadowMap, SamplerState samp, float4x4 shadowViewProj, float3 fragPosW, float2 uv, uint index);

uint CalcShiftedShadowValueF(float percent, uint value, uint index);
uint CalcShiftedShadowValueB(bool isHit, uint value, uint index);
uint GetShiftedShadowValue(uint value, uint index);

#endif // __SHADOW_DECLARATIONS_HLSLI__