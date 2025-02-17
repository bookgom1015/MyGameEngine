#ifndef __RANDOM_HLSLI__
#define __RANDOM_HLSLI__

#ifndef MAX_HALTON_SEQUENCE
#define MAX_HALTON_SEQUENCE 16
#endif 

static const float3 HALTON_SEQUENCE[MAX_HALTON_SEQUENCE] = {
	float3(0.5, 0.333333, 0.2),
	float3(0.25, 0.666667, 0.4),
	float3(0.75, 0.111111, 0.6),
	float3(0.125, 0.444444, 0.8),
	float3(0.625, 0.777778, 0.04),
	float3(0.375, 0.222222, 0.24),
	float3(0.875, 0.555556, 0.44) ,
	float3(0.0625, 0.888889, 0.64),
	float3(0.5625, 0.037037, 0.84),
	float3(0.3125, 0.37037, 0.08),
	float3(0.8125, 0.703704, 0.28),
	float3(0.1875, 0.148148, 0.48),
	float3(0.6875, 0.481482, 0.68),
	float3(0.4375, 0.814815, 0.88),
	float3(0.9375, 0.259259, 0.12),
	float3(0.03125, 0.592593, 0.32)
};

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

// Generates a seed for a random number generator from 2 inputs plus a backoff
uint InitRand(uint val0, uint val1, uint backoff = 16) {
	uint v0 = val0;
	uint v1 = val1;
	uint s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; ++n) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float NextRand(inout uint s) {
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 PerpendicularVector(float3 u) {
	float3 a = abs(u);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 CosHemisphereSample(inout uint seed, float3 hitNorm) {
	// Get 2 random numbers to select our sample with
	float2 randVal = float2(NextRand(seed), NextRand(seed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = PerpendicularVector(hitNorm);
	float3 tangent = cross(bitangent, hitNorm);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}

#endif // __RANDOM_HLSLI__