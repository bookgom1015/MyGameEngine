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
// Declarations
//
#include "BRDF_Declarations.hlsli"
#include "Shadow_Declarations.hlsli"
#include "Random_Declarations.hlsli"
#include "Conversion_Declarations.hlsli"
#include "Packaging_Declarations.hlsli"
#include "FloatPrecision_Declarations.hlsli"

//
// Implementations
//
#include "BRDF_Implementations.hlsli"
#include "Shadow_Implementations.hlsli"
#include "Random_Implementations.hlsli"
#include "Conversion_Implementations.hlsli"
#include "Packaging_Implementations.hlsli"
#include "FloatPrecision_Implementations.hlsli"

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

uint GetCubeFaceIndex(float3 direction) {
    float3 absDir = abs(direction);
    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
		return (direction.x > 0.0) ? 0 : 1; // +X : -X
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
		return (direction.y > 0.0) ? 2 : 3; // +Y : -Y
    else
		return (direction.z > 0.0) ? 4 : 5; // +Z : -Z    
}

// Convert normalized direction to UV coordinates for the 2D texture
float2 ConvertDirectionToUV(float3 dir) {	
	const float absX = abs(dir.x);
	const float absY = abs(dir.y);
	const float absZ = abs(dir.z);

	const float minX = min(dir.x, -1e-6);
	const float minY = min(dir.y, -1e-6);
	const float minZ = min(dir.z, -1e-6);

	float u, v;

	// Check which face the vector corresponds to
	if (absX >= absY && absX >= absZ) {
		// +X or -X face
		if (dir.x > 0) {
			u = 0.5 * (-dir.z / dir.x + 1.0);
			v = 0.5 * (-dir.y / dir.x + 1.0);
		}
		else {
			u = 0.5 * ( dir.z / -minX + 1.0);
			v = 0.5 * ( dir.y /  minX + 1.0);
		}
	}
	else if (absY >= absX && absY >= absZ) {
		// +Y or -Y face
		if (dir.y > 0) {
			u = 0.5 * ( dir.x / dir.y + 1.0);
			v = 0.5 * (-dir.z / dir.y + 1.0);
		}
		else {
			u = 0.5 * ( dir.x / -minY + 1.0);
			v = 0.5 * ( dir.z /  minY + 1.0);
		}
	}
	else {
		// +Z or -Z face
		if (dir.z > 0) {
			u = 0.5 * ( dir.x / dir.z + 1.0);
			v = 0.5 * (-dir.y / dir.z + 1.0);
		}
		else {
			u = 0.5 * (dir.x / minZ + 1.0);
			v = 0.5 * (dir.y / minZ + 1.0);
		}
	}

	return float2(u, v);
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

float ColorVariance(float4 lval, float4 rval) {
	float3 diff = (lval - rval).rgb;
	float variance = sqrt(dot(diff, diff)) * 0.577350269189;
	return variance;
}

#endif // __SHADINGHELPERS_HLSLI__