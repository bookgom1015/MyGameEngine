#ifndef __EDGESTOPPINGFILTER_GAUSSIAN3X3_CS_HLSL
#define __EDGESTOPPINGFILTER_GAUSSIAN3X3_CS_HLSL

#ifndef HLSL
#define HLSL
#endif

#ifndef GAUSSIAN_KERNEL_3X3
#define GAUSSIAN_KERNEL_3X3
#endif 

#ifdef VT_FLOAT4
#include "AtrousWaveletTransformFilterCS_Color.hlsli"
#else
#include "AtrousWaveletTransformFilterCS_Contrast.hlsli"
#endif

#endif // __EDGESTOPPINGFILTER_GAUSSIAN3X3_CS_HLSL