#include "Blur.h"

float* Blur::CalcGaussWeights(float sigma) {
	float twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	int blurRadius = static_cast<int>(ceil(2.0f * sigma));

	if (blurRadius > MaxBlurRadius) return nullptr;

	int size = 2 * blurRadius + 1;
	float* weights = new float[size];

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i) {
		float x = static_cast<float>(i);

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < size; ++i)
		weights[i] /= weightSum;

	return weights;
}