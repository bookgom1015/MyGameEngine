#pragma once

#include <cmath>
#include <Windows.h>

namespace Blur {
	const INT MaxBlurRadius = 17;

	FLOAT* CalcGaussWeights(FLOAT sigma);
}