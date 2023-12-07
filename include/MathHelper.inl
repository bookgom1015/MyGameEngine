#ifndef __MATHHELPER_INL__
#define __MATHHELPER_INL__

FLOAT MathHelper::Sin(FLOAT t) {
	return sinf(t);
}

FLOAT MathHelper::ASin(FLOAT t) {
	return asinf(t);
}

FLOAT MathHelper::Cos(FLOAT t) {
	return cosf(t);
}

FLOAT MathHelper::ACos(FLOAT t) {
	return acosf(t);
}

FLOAT MathHelper::Tan(FLOAT t) {
	return tanf(t);
}

FLOAT MathHelper::ATan2(FLOAT x, FLOAT y) {
	return atan2f(y, x);
}

constexpr FLOAT MathHelper::DegreesToRadians(FLOAT degrees) {
	return degrees * DegToRad;
}

constexpr FLOAT MathHelper::RadiansToDegrees(FLOAT radians) {
	return radians * RadToDeg;
}

FLOAT MathHelper::RandF() {
	return static_cast<FLOAT>(rand()) / static_cast<FLOAT>(RAND_MAX);
}

FLOAT MathHelper::RandF(FLOAT a, FLOAT b) {
	return a + RandF() * (b - a);
}

INT MathHelper::Rand(INT a, INT b) {
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
T MathHelper::Lerp(const T& a, const T& b, FLOAT t) {
	return a + (b - a)*t;
}

template<typename T>
T MathHelper::Clamp(const T& x, const T& low, const T& high) {
	return x < low ? low : (x > high ? high : x);
}

FLOAT MathHelper::Abs(FLOAT param) {
	return static_cast<FLOAT>(fabs(param));
}

constexpr BOOL MathHelper::IsZero(FLOAT value) {
	return value * value < Epsilon * Epsilon;
}

constexpr BOOL MathHelper::IsNotZero(FLOAT value) {
	return !IsZero(value);
}

BOOL MathHelper::IsEqual(FLOAT a, FLOAT b) {
	return Abs(a - b) < Epsilon;
}

BOOL MathHelper::IsNotEqual(FLOAT a, FLOAT b) {
	return Abs(a - b) >= Epsilon;
}

BOOL MathHelper::IsEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs) {
	return IsEqual(lhs.x, rhs.x) && IsEqual(lhs.y, rhs.y);
}

BOOL MathHelper::IsNotEqual(const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs) {
	return IsNotEqual(lhs, rhs);
}

BOOL MathHelper::IsEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs) {
	return IsEqual(lhs.x, rhs.x) && IsEqual(lhs.y, rhs.y) && IsEqual(lhs.z, rhs.z);
}

BOOL MathHelper::IsNotEqual(const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs) {
	return IsNotEqual(lhs, rhs);
}

BOOL MathHelper::IsEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs) {
	return IsEqual(lhs.x, rhs.x) && IsEqual(lhs.y, rhs.y) && IsEqual(lhs.z, rhs.z) && IsEqual(lhs.w, rhs.w);
}

BOOL MathHelper::IsNotEqual(const DirectX::XMFLOAT4& lhs, const DirectX::XMFLOAT4& rhs) {
	return IsNotEqual(lhs, rhs);
}

DirectX::PackedVector::XMFLOAT3PK MathHelper::PackXMFLOAT3(const DirectX::XMFLOAT3& v) {
	return DirectX::PackedVector::XMFLOAT3PK(v.x, v.y, v.z);
}

#endif // __MATHHELPER_INL__