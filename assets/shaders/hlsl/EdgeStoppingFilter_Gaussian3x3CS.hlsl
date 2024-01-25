#ifndef __EDGESTOPPINGFILTER_GAUSSIAN3X3_CS_HLSL
#define __EDGESTOPPINGFILTER_GAUSSIAN3X3_CS_HLSL

#ifndef HLSL
#define HLSL
#endif

#ifndef GAUSSIAN_KERNEL_3X3
#define GAUSSIAN_KERNEL_3X3
#endif 

#ifdef VT_FLOAT4
#include "AtrousWaveletTransformFilterCS_F4.hlsli"
#else
#include "AtrousWaveletTransformFilterCS_F1.hlsli"
#endif

#endif // __EDGESTOPPINGFILTER_GAUSSIAN3X3_CS_HLSL