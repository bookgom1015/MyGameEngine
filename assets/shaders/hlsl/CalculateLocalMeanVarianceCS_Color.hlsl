#ifndef __CALCULATELOCALMEANVARIANCECS_HLSL__
#define __CALCULATELOCALMEANVARIANCECS_HLSL__

// ---- Descriptions -------------------------------------------------------------------
// Calculate local mean and variance via a separable kernel and using wave intrinsics.
// Requirments:
//  - Wave lane size 16 or higher
//  - WaveReadLaneAt() with any to any to wave read lane support
// Supports:
//  - Up to 9x9 kernels
//  - Checkerboard on/off input. If enabled, outputs only for active pixels.
//     Active pixel is a pixel on the checkerboard pattern and has a valid /
//     generated value for it. The kernel is stretched in y direction
//     to sample only from active pixels.
// -------------------------------------------------------------------------------------

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "RTAO.hlsli"
#include "SVGF.hlsli"
#include "ValuePackaging.hlsli"
#include "RaytracedReflection.hlsli"

ConstantBuffer<ConstantBuffer_CalcLocalMeanVariance> cb_LocalMeanVar : register(b0);

Texture2D<SVGF::ValueMapFormat_HDR>				gi_Value				: register(t0);
RWTexture2D<SVGF::LocalMeanVarianceMapFormat>	go_LocalMeanVariance	: register(u0);

// Group shared memory cache for the row aggregated results.
groupshared uint2 PackedRowResultCache[16][8];	// 16bit float valueSum, squared valueSum
groupshared uint NumValuesCache[16][8];

// Adjust an index to a pixel that had a valid value generated for it.
// Inactive pixel indices get increased by 1 in the y direction.
int2 GetActivePixelIndex(int2 pixel) {
	bool isEvenPixel = ((pixel.x + pixel.y) & 1) == 0;
	return cb_LocalMeanVar.CheckerboardSamplingEnabled && cb_LocalMeanVar.EvenPixelActivated != isEvenPixel ? pixel + int2(0, 1) : pixel;
}

// Load up to 16x16 pixels and filter them horizontally.
// The output is cached in shared memory and contains NumRows x 8 results.
void FilterHorizontally(uint2 Gid, uint GI) {
	const uint2 GroupDim = uint2(8, 8);
	const uint NumValuesToLoadPerRowOrColumn = GroupDim.x + (cb_LocalMeanVar.KernelWidth - 1);

	// Processes the thread group as row-major 4x16, where each sub group of 16 threads processes one row.
	// Each thread loads up to 4 values, with the subgroups loading rows interleaved.
	// Loads up to 4x16x4 == 256 input values.
	uint2 GTid4x16_row0 = uint2(GI % 16, GI / 16);
	const int2 KernelBasePixel = (Gid * GroupDim - int(cb_LocalMeanVar.KernelRadius)) * int2(1, cb_LocalMeanVar.PixelStepY);
	const uint NumRowsToLoadPerThread = 4;
	const uint RowBaseWaveLaneIndex = (WaveGetLaneIndex() / 16) * 16;
		
	[unroll]
	for (uint i = 0; i < NumRowsToLoadPerThread; ++i) {
		uint2 GTid4x16 = GTid4x16_row0 + uint2(0, i * 4);
		if (GTid4x16.y >= NumValuesToLoadPerRowOrColumn) {
			if (GTid4x16.x < GroupDim.x) NumValuesCache[GTid4x16.y][GTid4x16.x] = 0;
			break;
		}

		// Load all the contributing columns for each row.
		int2 pixel = GetActivePixelIndex(KernelBasePixel + GTid4x16 * int2(1, cb_LocalMeanVar.PixelStepY));
		float4 value = 0;

		// The lane is out of bounds of the GroupDim * kernel, but could be within bounds of the input texture, so don't read it form the texture.
		// However, we need to keep it as an active lane for a below split sum.
		if (GTid4x16.x < NumValuesToLoadPerRowOrColumn && IsWithinBounds(pixel, cb_LocalMeanVar.TextureDim))
			value = gi_Value[pixel];

		// Filter the values for the first GroupDim columns.
		{
			// Accumulate for the whole kernel width.
			float4 valueSum = 0;
			float4 squaredValueSum = 0;
			uint numValues = 0;

			// Since a row uses 16 lanes, but we only need to calculate the aggregate for the first half (8) lanes,
			// split the kernel wide aggregation among the first 8 and the second 8 lanes, and then combine them.

			// Initialize the first 8 lanes to the first cell contribution of the kernel.
			// This covers the remainder of 1 in cb_LocalMeanVar.KernelWidth / 2 used in the loop below.
			if (GTid4x16.x < GroupDim.x && value.a != RaytracedReflection::InvalidReflectionAlphaValue) {
				valueSum = value;
				squaredValueSum = value * value;
				++numValues;
			}

			// Get the lane index that has the first value for a kernel in this lane.
			uint RowKernelStartLaneIndex = RowBaseWaveLaneIndex + 1 // Skip over the already accumulated firt cell of kernel.
				+ (GTid4x16.x < GroupDim.x ? GTid4x16.x : (GTid4x16.x - GroupDim.x) + cb_LocalMeanVar.KernelRadius);

			for (uint c = 0; c < cb_LocalMeanVar.KernelRadius; ++c) {
				uint laneToReadFrom = RowKernelStartLaneIndex + c;
				float4 cValue = WaveReadLaneAt(value, laneToReadFrom);

				if (cValue.a != RaytracedReflection::InvalidReflectionAlphaValue) {
					valueSum += cValue;
					squaredValueSum += cValue * cValue;
					++numValues;
				}
			}

			// Combine the sub-results.
			uint laneToReadFrom = min(WaveGetLaneCount() - 1, RowBaseWaveLaneIndex + GTid4x16.x + GroupDim.x);
			valueSum += WaveReadLaneAt(valueSum, laneToReadFrom);
			squaredValueSum += WaveReadLaneAt(squaredValueSum, laneToReadFrom);
			numValues += WaveReadLaneAt(numValues, laneToReadFrom);

			// Store only the valid results, i.e. first GroupDim columns.
			if (GTid4x16.x < GroupDim.x) {
				PackedRowResultCache[GTid4x16.y][GTid4x16.x] = uint2(Float4ToUint(valueSum), Float4ToUint(squaredValueSum));
				NumValuesCache[GTid4x16.y][GTid4x16.x] = numValues;
			}
		}
	}
}

void FilterVertically(uint2 DTid, uint2 GTid) {
	float4 valueSum = 0;
	float4 squaredValueSum = 0;
	uint numValues = 0;

	uint2 pixel = GetActivePixelIndex(int2(DTid.x, DTid.y * cb_LocalMeanVar.PixelStepY));

	float4 val1, val2;
	// Accumulate for the whole kernel.
	for (uint r = 0; r < cb_LocalMeanVar.KernelWidth; ++r) {
		uint rowID = GTid.y + r;
		uint rNumValues = NumValuesCache[rowID][GTid.x];

		if (rNumValues > 0) {
			uint2 unpackedRowSum = PackedRowResultCache[rowID][GTid.x];
			float4 rValueSum = UintToFloat4(unpackedRowSum.x);
			float4 rSquaredValueSum = UintToFloat4(unpackedRowSum.y);

			valueSum += rValueSum;
			squaredValueSum += rSquaredValueSum;
			numValues += rNumValues;
		}
	}

	// Calculate mean and variance.
	float invN = 1.0 / max(numValues, 1);
	float4 mean = invN * valueSum;

	// Apply Bessel's correction to the estimated variance, multiply by N/N-1,
	// since the true population mean is not known; it is only estimated as the sample mean.
	float besselCorrection = numValues / float(max(numValues, 2) - 1);
	float3 diff = (squaredValueSum - mean * mean).rgb;
	float variance = besselCorrection * (invN * sqrt(dot(diff, diff)) * 0.577350269189);

	variance = max(0, variance); // Ensure variance doesn't go negative due to imprecision.

	uint packed = Float4ToUint(mean);
	go_LocalMeanVariance[pixel] = numValues > 0 ? float2(packed, variance) : RaytracedReflection::InvalidReflectionAlphaValue;
}

[numthreads(SVGF::Default::ThreadGroup::Width, SVGF::Default::ThreadGroup::Height, 1)]
void CS(uint2 Gid : SV_GroupID, uint2 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex, uint2 DTid : SV_DispatchThreadID) {
	FilterHorizontally(Gid, GI);
	GroupMemoryBarrierWithGroupSync();

	FilterVertically(DTid, GTid);
}

#endif // __CALCULATELOCALMEANVARIANCECS_HLSL__