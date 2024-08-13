#ifndef __PACKAGING_IMPLEMENTATIONS_HLSLI__
#define __PACKAGING_IMPLEMENTATIONS_HLSLI__

uint Float2ToHalf(float2 val) {
	uint result = 0;
	result = f32tof16(val.x);
	result |= f32tof16(val.y) << 16;
	return result;
}

float2 HalfToFloat2(uint val) {
	float2 result;
	result.x = f16tof32(val);
	result.y = f16tof32(val >> 16);
	return result;
}

uint Float4ToUint(float4 val) {
	uint result = 0;
	result = asuint(val.x);
	result |= asuint(val.y) << 8;
	result |= asuint(val.z) << 16;
	result |= asuint(val.w) << 24;
	return result;
}

float4 UintToFloat4(uint val) {
	float4 result = 0;
	result.x = val;
	result.y = val >> 8;
	result.z = val >> 16;
	result.w = val >> 24;
	return result;
}

float2 OctWrap(float2 v) {
	return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}

float2 EncodeNormal(float3 n) {
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
	n.xy = n.xy * 0.5 + 0.5;
	return n.xy;
}

float3 DecodeNormal(float2 f) {
	f = f * 2.0 - 1.0;

	// https://twitter.com/Stubbesaurus/status/937994790553227264
	float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
	float t = saturate(-n.z);
	n.xy += n.xy >= 0.0 ? -t : t;
	return normalize(n);
}

uint Pack_R8_FLOAT(float r) {
	return clamp(round(r * 255), 0, 255);
}

float Unpack_R8_FLOAT(uint r) {
	return (r & 0xFF) / 255.0;
}

uint Pack_R8G8_to_R16_UINT(uint r, uint g) {
	return (r & 0xff) | ((g & 0xff) << 8);
}

void Unpack_R16_to_R8G8_UINT(uint v, out uint r, out uint g) {
	r = v & 0xFF;
	g = (v >> 8) & 0xFF;
}

uint Pack_R8G8B16_FLOAT(float3 rgb) {
	uint r = Pack_R8_FLOAT(rgb.r);
	uint g = Pack_R8_FLOAT(rgb.g) << 8;
	uint b = f32tof16(rgb.b) << 16;
	return r | g | b;
}

float3 Unpack_R8G8B16_FLOAT(uint rgb) {
	float r = Unpack_R8_FLOAT(rgb);
	float g = Unpack_R8_FLOAT(rgb >> 8);
	float b = f16tof32(rgb >> 16);
	return float3(r, g, b);
}

uint NormalizedFloat3ToByte3(float3 v) {
	return
		(uint(v.x * 255) << 16) +
		(uint(v.y * 255) << 8) +
		uint(v.z * 255);
}

float3 Byte3ToNormalizedFloat3(uint v) {
	return float3(
		(v >> 16) & 0xff,
		(v >> 8) & 0xff,
		v & 0xff) / 255;
}

uint EncodeNormalDepth_N16D16(float3 normal, float depth) {
	float3 encodedNormalDepth = float3(EncodeNormal(normal), depth);
	return Pack_R8G8B16_FLOAT(encodedNormalDepth);
}

void DecodeNormalDepth_N16D16(uint packedEncodedNormalAndDepth, out float3 normal, out float depth) {
	float3 encodedNormalDepth = Unpack_R8G8B16_FLOAT(packedEncodedNormalAndDepth);
	normal = DecodeNormal(encodedNormalDepth.xy);
	depth = encodedNormalDepth.z;
}

uint EncodeNormalDepth(float3 normal, float depth) {
	return EncodeNormalDepth_N16D16(normal, depth);
}

void DecodeNormalDepth(uint encodedNormalDepth, out float3 normal, out float depth) {
	DecodeNormalDepth_N16D16(encodedNormalDepth, normal, depth);
}

void DecodeNormal(uint encodedNormalDepth, out float3 normal) {
	float depthDummy;
	DecodeNormalDepth_N16D16(encodedNormalDepth, normal, depthDummy);
}

void DecodeDepth(uint encodedNormalDepth, out float depth) {
	float3 normalDummy;
	DecodeNormalDepth_N16D16(encodedNormalDepth, normalDummy, depth);
}

void UnpackEncodedNormalDepth(uint packedEncodedNormalDepth, out float2 encodedNormal, out float depth) {
	float3 encodedNormalDepth = Unpack_R8G8B16_FLOAT(packedEncodedNormalDepth);
	encodedNormal = encodedNormalDepth.xy;
	depth = encodedNormalDepth.z;
}

#endif // __PACKAGING_IMPLEMENTATIONS_HLSLI__