#ifndef __SHADINGHELPERS_HLSLI__
#define __SHADINGHELPERS_HLSLI__

#define INFINITY (1.0/0.0)

#define FLT_EPSILON     1.192092896e-07 // Smallest number such that 1.0 + FLT_EPSILON != 1.0
#define FLT_MIN         1.175494351e-38 
#define FLT_MAX         3.402823466e+38 
#define FLT_10BIT_MIN   6.1e-5
#define FLT_10BIT_MAX   6.5e4
#define PI              3.1415926535897f

//
// Trowbridge-Reitz GGX 
// 
float DistributionGGX(float3 N, float3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1) + 1);
	denom = PI * denom * denom;

	return num / denom;
}

//
// Smith's method with Schlick-GGX 
// 
// k is a remapping of ес based on whether using the geometry function 
//  for either direct lighting or IBL lighting.
float GeometryShlickGGX(float NdotV, float roughness) {
	float a = (roughness + 1);
	float k = (a * a) / 8;

	float num = NdotV;
	float denom = NdotV * (1 - k) + k;

	return num / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
	float NdotV = max(dot(N, V), 0);
	float NdotL = max(dot(N, L), 0);

	float ggx1 = GeometryShlickGGX(NdotV, roughness);
	float ggx2 = GeometryShlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

float GeometryShlickGGX_IBL(float NdotV, float roughness) {
	float a = roughness;
	float k = (a * a) / 2;

	float num = NdotV;
	float denom = NdotV * (1 - k) + k;

	return num / denom;
}
float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness) {
	float NdotV = max(dot(N, V), 0);
	float NdotL = max(dot(N, L), 0);

	float ggx1 = GeometryShlickGGX_IBL(NdotV, roughness);
	float ggx2 = GeometryShlickGGX_IBL(NdotL, roughness);

	return ggx1 * ggx2;
}

// the Fresnel Schlick approximation
float3 FresnelSchlick(float cos, float3 F0) {
	return F0 + (1 - F0) * pow(max((1 - cos), 0), 5);
}

float3 FresnelSchlickRoughness(float cos, float3 F0, float roughness) {
	return F0 + (max((float3)(1 - roughness), F0) - F0) * pow(max(1 - cos, 0), 5);
}

float RadicalInverse_VdC(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N) {
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness) {
	float a = roughness * roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

float CalcShadowFactor(Texture2D<float> shadowMap, SamplerComparisonState sampComp, float4 shadowPosH) {
	shadowPosH.xyz /= shadowPosH.w;

	float depth = shadowPosH.z;

	uint width, height, numMips;
	shadowMap.GetDimensions(0, width, height, numMips);

	float dx = 1.0f / (float)width;

	float percentLit = 0.0f;
	const float2 offsets[9] = {
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
	};

	[unroll]
	for (int i = 0; i < 9; ++i) {
		percentLit += shadowMap.SampleCmpLevelZero(sampComp, shadowPosH.xy + offsets[i], depth);
	}

	return percentLit / 9.0f;
}

float NdcDepthToViewDepth(float z_ndc, float4x4 proj) {
	// z_ndc = A + B/viewZ, where proj[2,2]=A and proj[3,2]=B.
	float viewZ = proj[3][2] / (z_ndc - proj[2][2]);
	return viewZ;
}

float2 CalcVelocity(float4 curr_pos, float4 prev_pos) {
	curr_pos.xy = (curr_pos.xy + (float2)1.0f) / 2.0f;
	curr_pos.y = 1.0f - curr_pos.y;

	prev_pos.xy = (prev_pos.xy + (float2)1.0f) / 2.0f;
	prev_pos.y = 1.0f - prev_pos.y;

	return (curr_pos - prev_pos).xy;
}

// Determines how much the sample point q occludes the point p as a function of distZ.
float OcclusionFunction(float distZ, float epsilon, float fadeStart, float fadeEnd) {
	//
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.
	//
	// We use the following function to determine the occlusion.  
	// 
	//
	//       1.0     -------------.
	//               |           |  .
	//               |           |    .
	//               |           |      . 
	//               |           |        .
	//               |           |          .
	//               |           |            .
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps          z0            z1        
	//
	float occlusion = 0.0f;
	if (distZ > epsilon) {
		float fadeLength = fadeEnd - fadeStart;

		// Linearly decrease occlusion from 1 to 0 as distZ goes from gOcclusionFadeStart to gOcclusionFadeEnd.	
		occlusion = saturate((fadeEnd - distZ) / fadeLength);
	}

	return occlusion;
}

float3 BoxCubeMapLookup(float3 rayOrigin, float3 unitRayDir, float3 boxCenter, float3 boxExtents) {
	// Set the origin to box of center.
	float3 p = rayOrigin - boxCenter;

	// The formula for AABB's i-th plate ray versus plane intersection is as follows.
	//
	// t1 = (-dot(n_i, p) + h_i) / dot(n_i, d) = (-p_i + h_i) / d_i
	// t2 = (-dot(n_i, p) - h_i) / dot(n_i, d) = (-p_i - h_i) / d_i
	float3 t1 = (-p + boxExtents) / unitRayDir;
	float3 t2 = (-p - boxExtents) / unitRayDir;

	// Find the maximum value for each coordinate component.
	// We assume that ray is inside the box, so we only need to find the maximum value of the intersection parameter. 
	float3 tmax = max(t1, t2);

	// Find the minimum value of all components for tmax.
	float t = min(min(tmax.x, tmax.y), tmax.z);

	// To use a lookup vector for a cube map, 
	// create coordinate relative to center of box.
	return p + t * unitRayDir;
}

float rand_1_05(in float2 uv) {
	float2 noise = frac(sin(dot(uv, float2(12.9898, 78.233) * 2)) * 43758.5453);
	return abs(noise.x + noise.y) * 0.5;
}

float2 rand_2_10(in float2 uv) {
	float noiseX = frac(sin(dot(uv, float2(12.9898, 78.233) * 2)) * 43758.5453);
	float noiseY = sqrt(1 - noiseX * noiseX);
	return float2(noiseX, noiseY);
}

float2 rand_2_0004(in float2 uv) {
	float noiseX = frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
	float noiseY = frac(sin(dot(uv, float2(12.9898, 78.233) * 2)) * 43758.5453);
	return float2(noiseX, noiseY) * 0.004;
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

uint GetIndexOfValueClosestToReference(float ref, float2 values) {
	float2 delta = abs(ref - values);
	uint index = delta[1] < delta[0] ? 1 : 0;
	return index;
}

uint GetIndexOfValueClosestToReference(float ref, float4 values) {
	float4 delta = abs(ref - values);
	uint index = delta[1] < delta[0] ? 1 : 0;
	index = delta[2] < delta[index] ? 2 : index;
	index = delta[3] < delta[index] ? 3 : index;
	return index;
}

bool IsWithinBounds(int2 index, int2 dimensions) {
	return index.x >= 0 && index.y >= 0 && index.x < dimensions.x&& index.y < dimensions.y;
}

uint SmallestPowerOf2GreaterThan(uint x) {
	// Set all the bits behind the most significant non-zero bit in x to 1.
	// Essentially giving us the largest value that is smaller than the
	// next power of 2 we're looking for.
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	// Return the next power of two value.
	return x + 1;
}

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

// Remap partial depth derivatives at z0 from [1,1] pixel offset to a new pixel offset.
float2 RemapDdxy(float z0, float2 ddxy, float2 pixelOffset) {
	// Perspective correction for non-linear depth interpolation.
	// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation/visibility-problem-depth-buffer-depth-interpolation
	// Given a linear depth interpolation for finding z at offset q along z0 to z1
	//      z =  1 / (1 / z0 * (1 - q) + 1 / z1 * q)
	// and z1 = z0 + ddxy, where z1 is at a unit pixel offset [1, 1]
	// z can be calculated via ddxy as
	//
	//      z = (z0 + ddxy) / (1 + (1-q) / z0 * ddxy) 
	float2 z = (z0 + ddxy) / (1 + ((1 - pixelOffset) / z0) * ddxy);
	return sign(pixelOffset) * (z - z0);
}

// Returns float precision for a given float value.
// Values within (value -precision, value + precision) map to the same value. 
// Precision = exponentRange/MaxMantissaValue = (2^e+1 - 2^e) / (2^NumMantissaBits)
// Ref: https://blog.demofox.org/2017/11/21/floating-point-precision/
float FloatPrecision(float x, uint NumMantissaBits) {
	// Find the exponent range the value is in.
	uint nextPowerOfTwo = SmallestPowerOf2GreaterThan(x);
	float exponentRange = nextPowerOfTwo - (nextPowerOfTwo >> 1);

	float MaxMantissaValue = 1u << NumMantissaBits;

	return exponentRange / MaxMantissaValue;
}

float FloatPrecisionR10(float x) {
	return FloatPrecision(x, 5);
}

float FloatPrecisionR16(float x) {
	return FloatPrecision(x, 10);
}

float FloatPrecisionR32(float x) {
	return FloatPrecision(x, 23);
}

bool IsInRange(float val, float min, float max) {
	return (val >= min && val <= max);
}

// Returns an approximate surface dimensions covered in a pixel. 
// This is a simplified model assuming pixel to pixel view angles are the same.
// z - linear depth of the surface at the pixel
// ddxy - partial depth derivatives
// tan_a - tangent of a per pixel view angle 
float2 ApproximateProjectedSurfaceDimensionsPerPixel(float z, float2 ddxy, float tan_a) {
	// Surface dimensions for a surface parallel at z.
	float2 dx = tan_a * z;

	// Using Pythagorean theorem approximate the surface dimensions given the ddxy.
	float2 w = sqrt(dx * dx + ddxy * ddxy);

	return w;
}

/***************************************************************/
// Normal encoding
// Ref: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
float2 OctWrap(float2 v) {
	return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}

// Converts a 3D unit vector to a 2D vector with <0,1> range. 
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
/***************************************************************/



// Pack [0.0, 1.0] float to 8 bit uint. 
uint Pack_R8_FLOAT(float r) {
	return clamp(round(r * 255), 0, 255);
}

float Unpack_R8_FLOAT(uint r) {
	return (r & 0xFF) / 255.0;
}

// pack two 8 bit uint2 into a 16 bit uint.
uint Pack_R8G8_to_R16_UINT(uint r, uint g) {
	return (r & 0xff) | ((g & 0xff) << 8);
}

void Unpack_R16_to_R8G8_UINT(uint v, out uint r, out uint g) {
	r = v & 0xFF;
	g = (v >> 8) & 0xFF;
}


// Pack unsigned floating point, where 
// - rgb.rg are in [0, 1] range stored as two 8 bit uints.
// - rgb.b in [0, FLT_16_BIT_MAX] range stored as a 16bit float.
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

// Encode normal and depth with 16 bits allocated for each.
uint EncodeNormalDepth_N16D16(float3 normal, float depth) {
	float3 encodedNormalDepth = float3(EncodeNormal(normal), depth);
	return Pack_R8G8B16_FLOAT(encodedNormalDepth);
}


// Decoded 16 bit normal and 16bit depth.
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

float ColorVariance(float4 lval, float4 rval) {
	float3 diff = (lval - rval).rgb;
	float variance = sqrt(dot(diff, diff)) * 0.577350269189;
	return variance;
}

#endif // __SHADINGHELPERS_HLSLI__