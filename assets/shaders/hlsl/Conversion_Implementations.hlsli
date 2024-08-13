#ifndef __CONVERSION_IMPLEMENTATION_HSLLI__
#define __CONVERSION_IMPLEMENTATION_HSLLI__

uint FloatToByte(float value) {
	value = clamp(value, 0.0, 1.0);
	return (uint)(value * 255.0 + 0.5);
}

float ByteToFloat(uint value) {
	return value / 255.0;
}

float4 sRGBToLinear(float4 color) {
	return pow(color, 2.2);
}

float4 LinearTosRGB(float4 color) {
	return pow(color, 1 / 2.2);
}

float3 RgbToYuv(float3 rgb) {
	float y = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
	return float3(y, 0.493 * (rgb.b - y), 0.877 * (rgb.r - y));
}

float3 YuvToRgb(float3 yuv) {
	float y = yuv.x;
	float u = yuv.y;
	float v = yuv.z;

	return float3(
		y + 1.0 / 0.877 * v,
		y - 0.39393 * u - 0.58081 * v,
		y + 1.0 / 0.493 * u
		);
}

#endif // __CONVERSION_IMPLEMENTATION_HSLLI__