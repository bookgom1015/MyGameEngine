#ifndef __FLOATPRECISION_DECLARATIONS_HLSLI__
#define __FLOATPRECISION_DECLARATIONS_HLSLI__

uint SmallestPowerOf2GreaterThan(uint x);
// Returns float precision for a given float value.
// Values within (value -precision, value + precision) map to the same value. 
// Precision = exponentRange/MaxMantissaValue = (2^e+1 - 2^e) / (2^NumMantissaBits)
// Ref: https://blog.demofox.org/2017/11/21/floating-point-precision/
float FloatPrecision(float x, uint NumMantissaBits);
float FloatPrecisionR10(float x);
float FloatPrecisionR16(float x);
float FloatPrecisionR32(float x);

#endif // __FLOATPRECISION_HLSLI__