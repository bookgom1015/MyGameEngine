#ifndef __MATHHELPER_INL__
#define __MATHHELPER_INL__

float MathHelper::Sin(float t) {
	return sinf(t);
}

float MathHelper::ASin(float t) {
	return asinf(t);
}

float MathHelper::Cos(float t) {
	return cosf(t);
}

float MathHelper::ACos(float t) {
	return acosf(t);
}

float MathHelper::Tan(float t) {
	return tanf(t);
}

float MathHelper::ATan2(float x, float y) {
	return atan2f(y, x);
}

constexpr float MathHelper::DegreesToRadians(float degrees) {
	return degrees * DegToRad;
}

constexpr float MathHelper::RadiansToDegrees(float radians) {
	return radians * RadToDeg;
}

float MathHelper::RandF() {
	return (float)(rand()) / (float)RAND_MAX;
}

float MathHelper::RandF(float a, float b) {
	return a + RandF() * (b - a);
}

int MathHelper::Rand(int a, int b) {
	return a + rand() % ((b - a) + 1);
}

template<typename T>
T MathHelper::Min(const T& a, const T& b) {
	return a < b ? a : b;
}

template<typename T>
T MathHelper::Max(const T& a, const T& b) {
	return a > b ? a : b;
}

template<typename T>
T MathHelper::Lerp(const T& a, const T& b, float t) {
	return a + (b - a)*t;
}

template<typename T>
T MathHelper::Clamp(const T& x, const T& low, const T& high) {
	return x < low ? low : (x > high ? high : x);
}

float MathHelper::Abs(float param) {
	return (float)fabs(param);
}

constexpr bool MathHelper::IsZero(float value) {
	return value * value < Epsilon * Epsilon;
}

constexpr bool MathHelper::IsNotZero(float value) {
	return !IsZero(value);
}

bool MathHelper::IsEqual(float a, float b) {
	return Abs(a - b) < Epsilon;
}

bool MathHelper::IsNotEqual(float a, float b) {
	return Abs(a - b) >= Epsilon;
}

bool MathHelper::IsEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs) {
	return IsEqual(lhs.x, rhs.x) && IsEqual(lhs.y, rhs.y);
}

bool MathHelper::IsNotEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs) {
	return IsNotEqual(lhs, rhs);
}

bool MathHelper::IsEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs) {
	return IsEqual(lhs.x, rhs.x) && IsEqual(lhs.y, rhs.y) && IsEqual(lhs.z, rhs.z);
}

bool MathHelper::IsNotEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs) {
	return IsNotEqual(lhs, rhs);
}

bool MathHelper::IsEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs) {
	return IsEqual(lhs.x, rhs.x) && IsEqual(lhs.y, rhs.y) && IsEqual(lhs.z, rhs.z) && IsEqual(lhs.w, rhs.w);
}

bool MathHelper::IsNotEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs) {
	return IsNotEqual(lhs, rhs);
}

#endif // __MATHHELPER_INL__