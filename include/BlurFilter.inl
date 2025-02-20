#ifndef __BLURFILTER_INL__
#define __BLURFILTER_INL__

INT BlurFilter::CalcSize(FLOAT sigma) {
	const INT blurRadius = static_cast<INT>(ceil(2.f * sigma));
	if (blurRadius > MaxBlurRadius) return -1;

	return 2 * blurRadius + 1;
}

BOOL BlurFilter::CalcGaussWeights(FLOAT sigma, FLOAT weights[]) {
	FLOAT twoSigma2 = 2.f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	const INT blurRadius = static_cast<INT>(ceil(2.f * sigma));
	if (blurRadius > MaxBlurRadius) return FALSE;

	const INT size = 2 * blurRadius + 1;
	FLOAT weightSum = 0.f;

	for (INT i = -blurRadius; i <= blurRadius; ++i) {
		FLOAT x = static_cast<FLOAT>(i);

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (INT i = 0; i < size; ++i)
		weights[i] /= weightSum;

	return TRUE;
}

#endif // __BLURFILTER_INL__