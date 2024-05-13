#ifndef __DXRSHADINGHELPERS_HLSLI__
#define __DXRSHADINGHELPERS_HLSLI__

typedef BuiltInTriangleIntersectionAttributes Attributes;

void CalculateHitPosition(float4x4 proj, float4x4 invProj, float4x4 invView, float2 texDim, float depth, uint2 launchIndex, out float3 hitPosition) {
	float2 tex = (launchIndex + 0.5) / texDim;
	float4 posH = float4(tex.x * 2 - 1, (1 - tex.y) * 2 - 1, 0, 1);
	float4 posV = mul(posH, invProj);
	posV /= posV.w;

	float dv = NdcDepthToViewDepth(depth, proj);
	posV = (dv / posV.z) * posV;

	hitPosition = mul(float4(posV.xyz, 1), invView).xyz;
}

uint3 Load3x32BitIndices(uint offsetBytes, uint instID, ByteAddressBuffer indices) {
	return indices.Load3(offsetBytes);
}

// Retrieve hit world position.
float3 HitWorldPosition() {
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], Attributes attr) {
	return vertexAttribute[0] +
		attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float2 HitAttribute(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr) {
	return vertexAttribute[0] +
		attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, float3 eyePosW, float4x4 invViewProj, out float3 origin, out float3 direction) {
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0f - 1.0f;

	// Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0.0f, 1.0f), invViewProj);

	world.xyz /= world.w;
	origin = eyePosW;
	direction = normalize(world.xyz - origin);
}

#endif // __DXRSHADINGHELPERS_HLSLI__