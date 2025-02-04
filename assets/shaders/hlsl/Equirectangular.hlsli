#ifndef __EQUIRECTANGULAR_HLSLI__
#define __EQUIRECTANGULAR_HLSLI__

namespace Equirectangular {
	static const float2 InvATan = float2(0.1591, 0.3183);

	float2 SampleSphericalMap(float3 view) {
		float2 texc = float2(atan2(view.z, view.x), asin(view.y));
		texc *= InvATan;
		texc += 0.5;
		texc.y = 1 - texc.y;
		return texc;
	}

	float3 SphericalToCartesian(float2 sphericalCoord) {
		// Convert from spherical coordinates to texture coordinates.
		sphericalCoord.y = 1 - sphericalCoord.y;
		sphericalCoord -= 0.5;
		sphericalCoord /= InvATan;

		// Convert texture coordinates to 3D space coordinates.
		float3 cartesianCoord;
		cartesianCoord.x = cos(sphericalCoord.x) * cos(sphericalCoord.y);
		cartesianCoord.y = sin(sphericalCoord.y);
		cartesianCoord.z = sin(sphericalCoord.x) * cos(sphericalCoord.y);

		return cartesianCoord;
	}
}

#endif // __EQUIRECTANGULAR_HLSLI__