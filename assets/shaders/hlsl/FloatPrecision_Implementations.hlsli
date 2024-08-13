#ifndef __FLOATPRECISION_IMPLEMENTATIONS_HLSLI__
#define __FLOATPRECISION_IMPLEMENTATIONS_HLSLI__

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

#endif // __FLOATPRECISION_HLSLI__