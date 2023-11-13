#include "HashUtil.h"

size_t hu::hash_combine(size_t seed, size_t value) {
	return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}