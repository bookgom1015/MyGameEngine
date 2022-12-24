#pragma once

#include <cmath>

namespace Blur {
	const int MaxBlurRadius = 17;

	float* CalcGaussWeights(float sigma);
}