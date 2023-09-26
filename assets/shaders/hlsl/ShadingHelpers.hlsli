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
// k is a remapping of �� based on whether using the geometry function 
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
	return F0 + (1 - F0) * pow(saturate(1 - cos), 5);
}

float3 FresnelSchlickRoughness(float cos, float3 F0, float roughness) {
	return F0 + (max((float3)(1 - roughness), F0) - F0) * pow(saturate(1 - cos), 5);
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

uint GetIndexOfValueClosest(float ref, float2 values) {
	float2 delta = abs(ref - values);
	uint index = delta[1] < delta[0] ? 1 : 0;
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

float FloatPrecision(float x, uint numMantissaBits) {
	// Find the exponent range the value is in.
	uint nextPowerOfTwo = SmallestPowerOf2GreaterThan(x);
	float exponentRange = nextPowerOfTwo - (nextPowerOfTwo >> 1);

	float maxMantissaValue = 1u << numMantissaBits;

	return exponentRange / maxMantissaValue;
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

#endif // __SHADINGHELPERS_HLSLI__