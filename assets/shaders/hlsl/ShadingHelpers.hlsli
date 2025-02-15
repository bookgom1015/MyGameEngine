#ifndef __SHADINGHELPERS_HLSLI__
#define __SHADINGHELPERS_HLSLI__

#include "ShadingConstants.hlsli"

uint GetCubeFaceIndex(in float3 direction) {
    const float3 absDir = abs(direction);
    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
		return (direction.x > 0.f) ? 0 : 1; // +X : -X
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
		return (direction.y > 0.f) ? 2 : 3; // +Y : -Y
    else
		return (direction.z > 0.f) ? 4 : 5; // +Z : -Z    
}

// Convert normalized direction to UV coordinates for the 2D texture
float2 ConvertDirectionToUV(in float3 dir) {
	const float absX = abs(dir.x);
	const float absY = abs(dir.y);
	const float absZ = abs(dir.z);

	const float dirX = dir.x > 0.f ? dir.x : min(dir.x, -1e-6f);
	const float dirY = dir.y > 0.f ? dir.y : min(dir.y, -1e-6f);
	const float dirZ = dir.z > 0.f ? dir.z : min(dir.z, -1e-6f);

	float u, v;

	// Check which face the vector corresponds to
	if (absX >= absY && absX >= absZ) {
		// +X or -X face
		if (dir.x > 0.f) {
			u = 0.5f * (-dir.z / dirX + 1.f);
			v = 0.5f * (-dir.y / dirX + 1.f);
		}
		else {
			u = 0.5f * ( dir.z / -dirX + 1.f);
			v = 0.5f * ( dir.y /  dirX + 1.f);
		}
	}
	else if (absY >= absX && absY >= absZ) {
		// +Y or -Y face
		if (dir.y > 0.f) {
			u = 0.5f * (dir.x / dirY + 1.f);
			v = 0.5f * (dir.z / dirY + 1.f);
		}
		else {
			u = 0.5f * ( dir.x / -dirY + 1.f);
			v = 0.5f * ( dir.z /  dirY + 1.f);
		}
	}
	else {
		// +Z or -Z face
		if (dir.z > 0.f) {
			u = 0.5f * ( dir.x / dirZ + 1.f);
			v = 0.5f * (-dir.y / dirZ + 1.f);
		}
		else {
			u = 0.5f * (dir.x / dirZ + 1.f);
			v = 0.5f * (dir.y / dirZ + 1.f);
		}
	}

	return float2(u, v);
}

float NdcDepthToViewDepth(in float z_ndc, in float4x4 proj) {
	// z_ndc = A + B/viewZ, where proj[2,2]=A and proj[3,2]=B.
	const float viewZ = proj[3][2] / (z_ndc - proj[2][2]);
	return viewZ;
}

float NdcDepthToExpViewDepth(in float z_ndc, in float z_exp, in float near, in float far) {
	const float viewZ = pow(z_ndc, z_exp) * (far - near) + near;
	return viewZ;
}

float3 ThreadIdToNdc(in uint3 DTid, in uint3 dims) {
	float3 ndc = DTid;
	ndc += 0.5f;
	ndc *= float3(2.f / dims.x, -2.f / dims.y, 1.f / dims.z);
	ndc += float3(-1.f, 1.f, 0.f);
	return ndc;
}

float3 NdcToWorldPosition(in float3 ndc, in float depthV, in float4x4 invView, in float4x4 invProj) {
	float4 rayV = mul(float4(ndc, 1.f), invProj);
	rayV /= rayV.w;
	rayV /= rayV.z; // So as to set the z depth value to 1.

	const float4 posW = mul(float4(rayV.xyz * depthV, 1.f), invView);
	return posW.xyz;
}

float3 ThreadIdToWorldPosition(in uint3 DTid, in uint3 dims, in float z_exp, in float near, in float far, in float4x4 invView, in float4x4 invProj) {
	const float3 ndc = ThreadIdToNdc(DTid, dims);
	const float depthV = NdcDepthToExpViewDepth(ndc.z, z_exp, near, far);
	return NdcToWorldPosition(ndc, depthV, invView, invProj);
}

float2 CalcVelocity(in float4 curr_pos, in float4 prev_pos) {
	curr_pos.xy = (curr_pos.xy + (float2)1.f) / 2.f;
	curr_pos.y = 1.f - curr_pos.y;

	prev_pos.xy = (prev_pos.xy + (float2)1.f) / 2.f;
	prev_pos.y = 1.f - prev_pos.y;

	return (curr_pos - prev_pos).xy;
}

// Determines how much the sample point q occludes the point p as a function of distZ.
float OcclusionFunction(in float distZ, in float epsilon, in float fadeStart, in float fadeEnd) {
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
	float occlusion = 0.f;
	if (distZ > epsilon) {
		const float fadeLength = fadeEnd - fadeStart;

		// Linearly decrease occlusion from 1 to 0 as distZ goes from gOcclusionFadeStart to gOcclusionFadeEnd.	
		occlusion = saturate((fadeEnd - distZ) / fadeLength);
	}

	return occlusion;
}

bool IsZeroMatrix(float4x4 mat) {
	return abs(dot(mat[0], 1)) + abs(dot(mat[1], 1)) + abs(dot(mat[2], 1)) + abs(dot(mat[3], 1)) < FLT_EPSILON;
}

bool IsNotZeroMatrix(float4x4 mat) {
	return abs(dot(mat[0], 1)) + abs(dot(mat[1], 1)) + abs(dot(mat[2], 1)) + abs(dot(mat[3], 1)) >= FLT_EPSILON;
}

#endif // __SHADINGHELPERS_HLSLI__