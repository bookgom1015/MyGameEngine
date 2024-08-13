#ifndef __CONVERSION_DECLARATIONS_HLSLI__
#define __CONVERSION_DECLARATIONS_HLSLI__

uint FloatToByte(float value);
float ByteToFloat(uint value);

float4 sRGBToLinear(float4 color);
float4 LinearTosRGB(float4 color);

float3 RgbToYuv(float3 rgb);
float3 YuvToRgb(float3 yuv);

#endif // __CONVERSION_DECLARATIONS_HLSLI__