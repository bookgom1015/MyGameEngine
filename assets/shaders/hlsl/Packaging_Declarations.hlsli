#ifndef __PACKAGING_DECLARATIONS_HLSLI__
#define __PACKAGING_DECLARATIONS_HLSLI__

uint Float2ToHalf(float2 val);
float2 HalfToFloat2(uint val);
uint Float4ToUint(float4 val);
float4 UintToFloat4(uint val);

/***************************************************************/
// Normal encoding
// Ref: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
float2 OctWrap(float2 v);

// Converts a 3D unit vector to a 2D vector with <0,1> range. 
float2 EncodeNormal(float3 n);
float3 DecodeNormal(float2 f);

// Pack [0.0, 1.0] float to 8 bit uint. 
uint Pack_R8_FLOAT(float r);
float Unpack_R8_FLOAT(uint r);

// pack two 8 bit uint2 into a 16 bit uint.
uint Pack_R8G8_to_R16_UINT(uint r, uint g);
void Unpack_R16_to_R8G8_UINT(uint v, out uint r, out uint g);

// Pack unsigned floating point, where 
// - rgb.rg are in [0, 1] range stored as two 8 bit uints.
// - rgb.b in [0, FLT_16_BIT_MAX] range stored as a 16bit float.
uint Pack_R8G8B16_FLOAT(float3 rgb);
float3 Unpack_R8G8B16_FLOAT(uint rgb);

uint NormalizedFloat3ToByte3(float3 v);
float3 Byte3ToNormalizedFloat3(uint v);

// Encode normal and depth with 16 bits allocated for each.
uint EncodeNormalDepth_N16D16(float3 normal, float depth);
// Decoded 16 bit normal and 16bit depth.
void DecodeNormalDepth_N16D16(uint packedEncodedNormalAndDepth, out float3 normal, out float depth);

uint EncodeNormalDepth(float3 normal, float depth);
void DecodeNormalDepth(uint encodedNormalDepth, out float3 normal, out float depth);
void DecodeNormal(uint encodedNormalDepth, out float3 normal);

void DecodeDepth(uint encodedNormalDepth, out float depth);

void UnpackEncodedNormalDepth(uint packedEncodedNormalDepth, out float2 encodedNormal, out float depth);

#endif // __PACKAGING_DECLARATIONS_HLSLI__