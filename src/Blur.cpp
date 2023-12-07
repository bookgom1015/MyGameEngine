#include "Blur.h"

FLOAT* Blur::CalcGaussWeights(FLOAT sigma) {
	FLOAT twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	INT blurRadius = static_cast<INT>(ceil(2.0f * sigma));

	if (blurRadius > MaxBlurRadius) return nullptr;

	INT size = 2 * blurRadius + 1;
	FLOAT* weights = new FLOAT[size];

	FLOAT weightSum = 0.0f;

	for (INT i = -blurRadius; i <= blurRadius; ++i) {
		FLOAT x = static_cast<FLOAT>(i);

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (INT i = 0; i < size; ++i)
		weights[i] /= weightSum;

	return weights;
}