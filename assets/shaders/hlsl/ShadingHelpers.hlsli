#ifndef __SHADINGHELPERS_HLSLI__
#define __SHADINGHELPERS_HLSLI__

#include "ShadingConstants.hlsli"

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

	const float dirX = dir.x > 0 ? dir.x : min(dir.x, -1e-6);
	const float dirY = dir.y > 0 ? dir.y : min(dir.y, -1e-6);
	const float dirZ = dir.z > 0 ? dir.z : min(dir.z, -1e-6);

	float u, v;

	// Check which face the vector corresponds to
	if (absX >= absY && absX >= absZ) {
		// +X or -X face
		if (dir.x > 0) {
			u = 0.5 * (-dir.z / dirX + 1.0);
			v = 0.5 * (-dir.y / dirX + 1.0);
		}
		else {
			u = 0.5 * ( dir.z / -dirX + 1.0);
			v = 0.5 * ( dir.y /  dirX + 1.0);
		}
	}
	else if (absY >= absX && absY >= absZ) {
		// +Y or -Y face
		if (dir.y > 0) {
			u = 0.5 * ( dir.x / dirY + 1.0);
			v = 0.5 * (-dir.z / dirY + 1.0);
		}
		else {
			u = 0.5 * ( dir.x / -dirY + 1.0);
			v = 0.5 * ( dir.z /  dirY + 1.0);
		}
	}
	else {
		// +Z or -Z face
		if (dir.z > 0) {
			u = 0.5 * ( dir.x / dirZ + 1.0);
			v = 0.5 * (-dir.y / dirZ + 1.0);
		}
		else {
			u = 0.5 * (dir.x / dirZ + 1.0);
			v = 0.5 * (dir.y / dirZ + 1.0);
		}
	}

	return float2(u, v);
}

float NdcDepthToViewDepth(float z_ndc, float4x4 proj) {
	// z_ndc = A + B/viewZ, where proj[2,2]=A and proj[3,2]=B.
	const float viewZ = proj[3][2] / (z_ndc - proj[2][2]);
	return viewZ;
}

float NdcDepthToExpViewDepth(float z_ndc, float z_exp, float near, float far) {
	const float viewZ = pow(z_ndc, z_exp) * (far - near) + near;
	return viewZ;
}

float3 ThreadIdToNdc(uint3 DTid, uint3 dims) {
	float3 ndc = DTid;
	ndc += 0.5;
	ndc *= float3(2.0 / dims.x, -2.0 / dims.y, 1.0 / dims.z);
	ndc += float3(-1, 1, 0);
	return ndc;
}

float3 NdcToWorldPosition(float3 ndc, float depthV, float4x4 invView, float4x4 invProj) {
	float4 rayV = mul(float4(ndc, 1), invProj);
	rayV /= rayV.w;
	rayV /= rayV.z; // So as to set the z depth value to 1.

	const float4 posW = mul(float4(rayV.xyz * depthV, 1), invView);
	return posW.xyz;
}

float3 ThreadIdToWorldPosition(uint3 DTid, uint3 dims, float z_exp, float near, float far, float4x4 invView, float4x4 invProj) {
	const float3 ndc = ThreadIdToNdc(DTid, dims);
	const float depthV = NdcDepthToExpViewDepth(ndc.z, z_exp, near, far);
	return NdcToWorldPosition(ndc, depthV, invView, invProj);
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

#endif // __SHADINGHELPERS_HLSLI__