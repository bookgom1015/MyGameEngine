#ifndef __GAUSSIANBLUR_HLSL__
#define __GAUSSIANBLUR_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Blur> cb_Blur		: register(b0);

BlurFilter_Default_RootConstants(b1)

Texture2D<GBuffer::NormalMapFormat>	gi_Normal	: register(t0);
Texture2D<GBuffer::DepthMapFormat>	gi_Depth	: register(t1);

Texture2D<float4>					gi_Input_F4	: register(t2);
Texture2D<float3>					gi_Input_F3	: register(t3);
Texture2D<float2>					gi_Input_F2	: register(t4);
Texture2D<float>					gi_Input_F1	: register(t5);

#if FT_F4
	typedef float4 PixelOut;
#elif FT_F3
	typedef float3 PixelOut;
#elif FT_F2
	typedef float2 PixelOut;
#else
	typedef float PixelOut;
#endif

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH  : SV_POSITION;
	float2 TexC  : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	return vout;
}

PixelOut PS(VertexOut pin) : SV_Target {
	// unpack into float array.
	const float blurWeights[12] = {
		cb_Blur.BlurWeights[0].x, cb_Blur.BlurWeights[0].y, cb_Blur.BlurWeights[0].z, cb_Blur.BlurWeights[0].w,
		cb_Blur.BlurWeights[1].x, cb_Blur.BlurWeights[1].y, cb_Blur.BlurWeights[1].z, cb_Blur.BlurWeights[1].w,
		cb_Blur.BlurWeights[2].x, cb_Blur.BlurWeights[2].y, cb_Blur.BlurWeights[2].z, cb_Blur.BlurWeights[2].w,
	};

	uint2 size;
	#if FT_F4
		gi_Input_F4.GetDimensions(size.x, size.y);
	#elif FT_F3
		gi_Input_F3.GetDimensions(size.x, size.y);
	#elif FT_F2
		gi_Input_F2.GetDimensions(size.x, size.y);
	#else
		gi_Input_F1.GetDimensions(size.x, size.y);
	#endif

	const float dx = 1.f / size.x;
	const float dy = 1.f / size.y;

	float2 texOffset;
	if (gHorizontal) texOffset = float2(dx, 0.f);
	else texOffset = float2(0.f, dy);

	// The center value always contributes to the sum.
	#if FT_F4
		float4 color = blurWeights[cb_Blur.BlurRadius] * gi_Input_F4.Sample(gsamLinearClamp, pin.TexC);
	#elif FT_F3
		float3 color = blurWeights[cb_Blur.BlurRadius] * gi_Input_F3.Sample(gsamLinearClamp, pin.TexC);
	#elif FT_F2
		float2 color = blurWeights[cb_Blur.BlurRadius] * gi_Input_F2.Sample(gsamLinearClamp, pin.TexC);
	#else
		float color = blurWeights[cb_Blur.BlurRadius] * gi_Input_F1.Sample(gsamLinearClamp, pin.TexC);
	#endif
	float totalWeight = blurWeights[cb_Blur.BlurRadius];

	float3 centerNormal;
	float centerDepth;
	if (gBilateral) {		
		centerNormal = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);
		centerDepth = NdcDepthToViewDepth(gi_Depth.Sample(gsamDepthMap, pin.TexC), cb_Blur.Proj);
	}

	[loop]
	for (int i = -cb_Blur.BlurRadius; i <= cb_Blur.BlurRadius; ++i)	{
		// We already added in the center weight.
		if (i == 0) continue;

		const float2 tex = pin.TexC + i * texOffset;

		if (gBilateral) {
			const float3 neighborNormal = normalize(gi_Normal.Sample(gsamLinearClamp, tex).xyz);
			const float neighborDepth = NdcDepthToViewDepth(gi_Depth.Sample(gsamDepthMap, tex), cb_Blur.Proj);

			//
			// If the center value and neighbor values differ too much (either in 
			// normal or depth), then we assume we are sampling across a discontinuity.
			// We discard such samples from the blur.
			//
			if (dot(neighborNormal, centerNormal) >= 0.75f && abs(neighborDepth - centerDepth) <= 1.f) {
				const float weight = blurWeights[i + cb_Blur.BlurRadius];

				// Add neighbor pixel to blur.
				#if FT_F4
					color += weight * gi_Input_F4.Sample(gsamLinearClamp, tex);
				#elif FT_F3
					color += weight * gi_Input_F3.Sample(gsamLinearClamp, tex);
				#elif FT_F2
					color += weight * gi_Input_F2.Sample(gsamLinearClamp, tex);
				#else
					color += weight * gi_Input_F1.Sample(gsamLinearClamp, tex);
				#endif

				totalWeight += weight;
			}
		}
		else {
			const float weight = blurWeights[i + cb_Blur.BlurRadius];

			#if FT_F4
				color += weight * gi_Input_F4.Sample(gsamLinearClamp, tex);
			#elif FT_F3
				color += weight * gi_Input_F3.Sample(gsamLinearClamp, tex);
			#elif FT_F2
				color += weight * gi_Input_F2.Sample(gsamLinearClamp, tex);
			#else
				color += weight * gi_Input_F1.Sample(gsamLinearClamp, tex);
			#endif

			totalWeight += weight;
		}
	}

	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}

#endif // __GAUSSIANBLUR_HLSL__
