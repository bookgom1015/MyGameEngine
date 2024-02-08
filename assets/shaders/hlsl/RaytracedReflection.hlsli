#ifndef __RAYTRACEDREFLECTION_HLSLI__
#define __RAYTRACEDREFLECTION_HLSLI__

namespace RaytracedReflection {
	static const float InvalidReflectionAlphaValue = -1;
	static const float RayHitDistanceOnMiss = 0;
	bool HasReflectionRayHitAnyGeometry(float tHit) {
		return tHit != RayHitDistanceOnMiss;
	}
}

#endif // __RAYTRACEDREFLECTION_HLSLI__