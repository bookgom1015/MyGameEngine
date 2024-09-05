#ifndef __DISOCCLUSIONBLUR3X3CS_HLSL__
#define __DISOCCLUSIONBLUR3X3CS_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Rtao.hlsli"
#include "RaytracedReflection.hlsli"

#define GAUSSIAN_KERNEL_3X3
#include "Kernels.hlsli"

cbuffer cbRootConstants : register(b0) {
	uint2	gTextureDim;
	uint	gStep;
	uint	gMaxStep;
};

Texture2D<GBuffer::DepthMapFormat>					gi_Depth		: register(t0);
Texture2D<SVGF::DisocclusionBlurStrengthMapFormat>	gi_BlurStrength	: register(t1);

Texture2D<GBuffer::RMSMapFormat>					gi_RMS			: register(t0, space1);

RWTexture2D<SVGF::ValueMapFormat_HDR>				gio_Value		: register(u0);

// Group shared memory cache for the row aggregated results.
static const uint NumValuesToLoadPerRowOrColumn = SVGF::Default::ThreadGroup::Width	+ (FilterKernel::Width - 1);
groupshared float PackedDepthCache[NumValuesToLoadPerRowOrColumn][8];
// 32bit float filtered value.
groupshared float4 FilteredResultCache[NumValuesToLoadPerRowOrColumn][8];
groupshared float4 PackedValueCache[NumValuesToLoadPerRowOrColumn][8];

// Find a dispatchThreadID with steps in between the group threads and groups interleaved to cover all pixels.
uint2 GetPixelIndex(uint2 Gid, uint2 GTid) {
	const uint2 GroupDim = uint2(8, 8);
	const uint2 GroupBase = (Gid / gStep) * GroupDim * gStep + Gid % gStep;
	const uint2 GroupThreadOffset = GTid * gStep;
	const uint2 sDTid = GroupBase + GroupThreadOffset;
	return sDTid;
}

// Load up to 16x16 pixels and filter them horizontally.
// The output is cached in Shared Memory and constants NumRows x 8 results.
void FilterHorizontally(uint2 Gid, uint GI) {
	const uint2 GroupDim = uint2(8, 8);

	// Processes the thread group as row-major 4x16, where each sub group of 16 threads processes one row.
	// Each thread loads up to 4 values, with the sub groups loading rows interleaved.
	// Loads up to 4x16x4 == 256 input values.
	const uint2 GTid4x16_row0 = uint2(GI % 16, GI / 16);
	const int2 GroupKernelBasePixel = GetPixelIndex(Gid, 0) - int(FilterKernel::Radius * gStep);
	const uint NumRowsToLoadPerThread = 4;
	const uint RowBaseWaveLaneIndex = (WaveGetLaneIndex() / 16) * 16;

	[unroll]
	for (uint i = 0; i < NumRowsToLoadPerThread; ++i) {
		uint2 GTid4x16 = GTid4x16_row0 + uint2(0, i * 4);
		if (GTid4x16.y >= NumValuesToLoadPerRowOrColumn) break;

		// Load all the contributing columns for each row.
		int2 pixel = GroupKernelBasePixel + GTid4x16 * gStep;
		float4 value = (float4)RaytracedReflection::InvalidReflectionAlphaValue;
		float depth = 0;

		// The lane is out of bounds of the GroupDim + kernel, but could be within bounds of the input texture,
		//  so don't read it from the texture.
		// However, we need to keep it as an active lane for a below split sum.
		if (GTid4x16.x < NumValuesToLoadPerRowOrColumn && IsWithinBounds(pixel, gTextureDim)) {
			value = gio_Value[pixel];
			depth = gi_Depth[pixel];
		}

		// Cache the kernel center values.
		if (IsInRange(GTid4x16.x, FilterKernel::Radius, FilterKernel::Radius + GroupDim.x - 1)) {
			int row = GTid4x16.y;
			int col = GTid4x16.x - FilterKernel::Radius;
			PackedValueCache[row][col] = value;
			PackedDepthCache[row][col] = depth;
		}

		// Filter the values for the first GroupDim columns.
		{
			// Accumulate for the whole kernel width.
			float4 weightedValueSum = 0;
			float4 gaussianWeightedValueSum = 0;
			float weightSum = 0;
			float gaussianWeightedSum = 0;

			// Since a row uses 16 lanes, but we only need to calculate the aggregate for the first half (8) lanes,
			//  split the kernel wide aggregation among the first 8 and the second 8 lanes, and then combine them.

			// Get the lane index that has the first value for a kernel in this lane.
			uint RowKernelStartLaneIndex =
				(RowBaseWaveLaneIndex + GTid4x16.x)
				- (GTid4x16.x < GroupDim.x ? 0 : GroupDim.x);

			// Get values for the kernel center.
			uint kcLaneIndex = RowKernelStartLaneIndex + FilterKernel::Radius;
			float4 kcValue = WaveReadLaneAt(value, kcLaneIndex);
			float kcDepth = WaveReadLaneAt(depth, kcLaneIndex);

			// Initialize the first 8 lanes to the center cell contribution of the kernel.
			// This covers the remainder of 1 in FilterKernel::Width / 2 used in the loop below.
			{
				if (GTid4x16.x < GroupDim.x && kcValue.a != RaytracedReflection::InvalidReflectionAlphaValue && kcDepth != GBuffer::InvalidNormDepthValue) {
					float w_h = FilterKernel::Kernel1D[FilterKernel::Radius];
					gaussianWeightedValueSum = w_h * kcValue;
					gaussianWeightedSum = w_h;
					weightedValueSum = gaussianWeightedValueSum;
					weightSum = w_h;
				}
			}

			// Second 8 lanes start just past the kernel center.
			uint KernelCellIndexOffset = GTid4x16.x < GroupDim.x ?
				0 : (FilterKernel::Radius + 1); // Skip over the already accumulated center cell of the kernel.

			// For all columns in the kernel.
			for (uint c = 0; c < FilterKernel::Radius; ++c) {
				uint kernelCellIndex = KernelCellIndexOffset + c;

				uint laneToReadFrom = RowKernelStartLaneIndex + kernelCellIndex;
				float4 cValue = WaveReadLaneAt(value, laneToReadFrom);
				float cDepth = WaveReadLaneAt(depth, laneToReadFrom);

				if (cValue.a != RaytracedReflection::InvalidReflectionAlphaValue && kcDepth != GBuffer::InvalidNormDepthValue && cDepth != GBuffer::InvalidNormDepthValue) {
					float w_h = FilterKernel::Kernel1D[kernelCellIndex];

					// Simple depth test with tolerance growing as the kernel radius increases.
					// Goal is to prevent values too far apart to blend together, while having
					//  the test being relaxed enough to get a strong blurring result.
					float depthThreshold = 0.05 + gStep * 0.001 * abs(int(FilterKernel::Radius) - c);
					float w_d = abs(kcDepth - cDepth) <= depthThreshold * kcDepth;
					float w = w_h * w_d;

					weightedValueSum += w * cValue;
					weightSum += w;
					gaussianWeightedValueSum += w_h * cValue;
					gaussianWeightedSum += w_h;
				}
			}

			// Combine the sub-results.
			uint laneToReadFrom = min(WaveGetLaneCount() - 1, RowBaseWaveLaneIndex + GTid4x16.x + GroupDim.x);
			weightedValueSum += WaveReadLaneAt(weightedValueSum, laneToReadFrom);
			weightSum += WaveReadLaneAt(weightSum, laneToReadFrom);
			gaussianWeightedValueSum += WaveReadLaneAt(gaussianWeightedValueSum, laneToReadFrom);
			gaussianWeightedSum += WaveReadLaneAt(gaussianWeightedSum, laneToReadFrom);

			// Store only the valid results, i.e. first GroupDim columns.
			if (GTid4x16.x < GroupDim.x) {
				float mag = sqrt(dot(gaussianWeightedSum, gaussianWeightedSum));
				float4 gaussianFilteredValue = mag > 1e-6 ? gaussianWeightedValueSum / gaussianWeightedSum : (float4)RaytracedReflection::InvalidReflectionAlphaValue;
				float4 filteredValue = weightSum > 1e-6 ? weightedValueSum / weightSum : gaussianFilteredValue;

				FilteredResultCache[GTid4x16.y][GTid4x16.x] = filteredValue;
			}
		}
	}
}

void FilterVertically(uint2 DTid, uint2 GTid, float blurStrength) {
	// Kernel center values.
	float4 kcValue = PackedValueCache[GTid.y + FilterKernel::Radius][GTid.x];
	float4 filteredValue = kcValue;
	float kcDepth = PackedDepthCache[GTid.y + FilterKernel::Radius][GTid.x];

	if (blurStrength >= 0.01 && kcDepth != GBuffer::InvalidNormDepthValue) {
		float4 weightedValueSum = 0;
		float4 gaussianWeightedValueSum = 0;
		float weightSum = 0;
		float gaussianWeightSum = 0;

		// For all rows in the kernel.
		[unroll]
		for (uint r = 0; r < FilterKernel::Width; ++r) {
			uint rowID = GTid.y + r;

			float rDepth = PackedDepthCache[rowID][GTid.x];
			float4 rFilteredValue = FilteredResultCache[rowID][GTid.x];

			if (rDepth != GBuffer::InvalidNormDepthValue && rFilteredValue.a != RaytracedReflection::InvalidReflectionAlphaValue) {
				float w_h = FilterKernel::Kernel1D[r];

				// Simple depth test with tolerance growing as the kernel radius increases.
				// Goal is to prevent values too far apart to blend together, while having 
				// the test being relaxed enough to get a strong blurring result.
				float depthThreshold = 0.05 + gStep * 0.001 * abs(int(FilterKernel::Radius) - int(r));
				float w_d = abs(kcDepth - rDepth) <= depthThreshold * kcDepth;
				float w = w_h * w_d;

				weightedValueSum += w * rFilteredValue;
				weightSum += w;
				gaussianWeightedValueSum += w_h * rFilteredValue;
				gaussianWeightSum += w_h;
			}
		}

		float mag = sqrt(dot(gaussianWeightSum, gaussianWeightSum));
		float4 gaussianFilteredValue = mag > 1e-6 ? gaussianWeightedValueSum / gaussianWeightSum : (float4)RaytracedReflection::InvalidReflectionAlphaValue;

		mag = sqrt(dot(weightSum, weightSum));
		filteredValue = weightSum > 1e-6 ? weightedValueSum / weightSum : gaussianFilteredValue;
		filteredValue = filteredValue.a != RaytracedReflection::InvalidReflectionAlphaValue ? lerp(kcValue, filteredValue, blurStrength) : filteredValue;
	}

	gio_Value[DTid] = filteredValue;
}

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 Gid : SV_GroupID, uint2 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex, uint2 DTid : SV_DispatchThreadID) {
	const float4 rms = gi_RMS[DTid];
	const float var_x = rms.r;

	const float exponent = 1.0 / 16.0;
	const float numer = pow(log(var_x + 1), exponent) * (gMaxStep - 1);
	const float denom = pow(log(2), exponent);
	const float limit = numer / denom;
	if (gStep > floor(limit)) return;

	const uint2 sDTid = GetPixelIndex(Gid, GTid);
	// Pass through if all pixels have 0 blur strength set.
	float blurStrength;
	{
		if (GI == 0) FilteredResultCache[0][0] = 0;
		GroupMemoryBarrierWithGroupSync();

		blurStrength = gi_BlurStrength[sDTid];

		const float MinBlurStrength = 0.01;
		const bool ValueNeedsFiltering = blurStrength >= MinBlurStrength;
		if (ValueNeedsFiltering) FilteredResultCache[0][0] = 1;

		GroupMemoryBarrierWithGroupSync();

		if (FilteredResultCache[0][0].a == 0) return;
	}

	FilterHorizontally(Gid, GI);
	GroupMemoryBarrierWithGroupSync();

	FilterVertically(sDTid, GTid, blurStrength);
}

#endif // __DISOCCLUSIONBLUR3X3CS_HLSL__